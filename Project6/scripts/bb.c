#define _GNU_SOURCE
#include "bb.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#ifndef BLOCK_BYTES
#define BLOCK_BYTES 64
#endif

#define BLOCK_BITS  (BLOCK_BYTES * 8)   // 512 bits

//  64-bit mixing / hashing 

static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline uint64_t hash64(uint64_t key, uint64_t seed) {
    uint64_t x = key ^ seed;
    return splitmix64(&x);
}

static inline void block_set_bit(uint8_t *block, uint32_t bit) {

    uint32_t byte = bit >> 3;
    uint32_t mask = 1u << (bit & 7u);
    block[byte] |= (uint8_t)mask;
}

static inline int block_test_bit(const uint8_t *block, uint32_t bit) {
    uint32_t byte = bit >> 3;
    uint32_t mask = 1u << (bit & 7u);
    return (block[byte] & (uint8_t)mask) != 0;
}

// m = -n ln(p) / (ln 2)^2
// k = (m/n) ln 2
static inline void choose_m_k(size_t n, double p, size_t *m_bits_out, uint32_t *k_out) {
    if (p <= 0.0) p = 1e-12;
    if (p >= 1.0) p = 0.999999;

    const double ln2 = 0.693147180559945309417232121458176568;
    double m = -(double)n * log(p) / (ln2 * ln2);
    if (m < (double)BLOCK_BITS) m = (double)BLOCK_BITS;

    size_t m_bits = (size_t)ceil(m);

    double kf = (m / (double)n) * ln2;
    uint32_t k = (uint32_t)llround(kf);
    if (k < 1) k = 1;
    if (k > 16) k = 16;

    *m_bits_out = m_bits;
    *k_out = k;
}

int blocked_bloom_init(blocked_bloom_t *bf, size_t n_keys, double target_fpr) {
    if (!bf || n_keys == 0) return EINVAL;

    size_t m_bits = 0;
    uint32_t k = 0;
    choose_m_k(n_keys, target_fpr, &m_bits, &k);

    // blocked bloom: number of 64B blocks
    size_t nblocks = (m_bits + BLOCK_BITS - 1) / BLOCK_BITS;
    if (nblocks == 0) nblocks = 1;

    // allocate contiguous array
    size_t bytes = nblocks * (size_t)BLOCK_BYTES;

    // align to cache line is nice but not required; calloc already zeroes.
    uint8_t *mem = (uint8_t*)calloc(1, bytes);
    if (!mem) return ENOMEM;

    bf->blocks = mem;
    bf->nblocks = nblocks;
    bf->k = k;
    bf->n_keys_hint = n_keys;
    bf->target_fpr = target_fpr;
    return 0;
}

int blocked_bloom_insert(blocked_bloom_t *bf, uint64_t key) {
    if (!bf || !bf->blocks || bf->nblocks == 0) return EINVAL;

    // Two independent hashes:
    // h1 selects block; h2 generates bit positions inside the block.
    uint64_t h1 = hash64(key, 0x123456789abcdef0ULL);
    uint64_t h2 = hash64(key, 0xfedcba9876543210ULL);

    size_t b = (size_t)(h1 % bf->nblocks);
    uint8_t *block = bf->blocks + b * (size_t)BLOCK_BYTES;

    // pos_i = (h2 + i * (h2>>32 | 1)) mod 512
    uint32_t step = (uint32_t)(h2 >> 32) | 1u;
    uint32_t x = (uint32_t)h2;

    for (uint32_t i = 0; i < bf->k; i++) {
        uint32_t bit = (x + i * step) & (BLOCK_BITS - 1u); 
        block_set_bit(block, bit);
    }
    return 0;
}

int blocked_bloom_query(const blocked_bloom_t *bf, uint64_t key) {
    if (!bf || !bf->blocks || bf->nblocks == 0) return 0;

    uint64_t h1 = hash64(key, 0x123456789abcdef0ULL);
    uint64_t h2 = hash64(key, 0xfedcba9876543210ULL);

    size_t b = (size_t)(h1 % bf->nblocks);
    const uint8_t *block = bf->blocks + b * (size_t)BLOCK_BYTES;

    uint32_t step = (uint32_t)(h2 >> 32) | 1u;
    uint32_t x = (uint32_t)h2;

    for (uint32_t i = 0; i < bf->k; i++) {
        uint32_t bit = (x + i * step) & (BLOCK_BITS - 1u);
        if (!block_test_bit(block, bit)) return 0; 
    }
    return 1; 
}

size_t blocked_bloom_bytes(const blocked_bloom_t *bf) {
    if (!bf || !bf->blocks) return 0;
    return bf->nblocks * (size_t)BLOCK_BYTES;
}

void blocked_bloom_free(blocked_bloom_t *bf) {
    if (!bf) return;
    free(bf->blocks);
    bf->blocks = NULL;
    bf->nblocks = 0;
    bf->k = 0;
    bf->n_keys_hint = 0;
    bf->target_fpr = 0.0;
}
