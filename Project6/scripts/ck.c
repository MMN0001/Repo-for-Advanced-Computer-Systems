#include "ck.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BUCKET_SIZE 4
#define MAX_KICKS 500
#define STASH_CAP 16

static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline uint32_t fingerprint(uint64_t key, uint32_t mask) {
    uint64_t x = key;
    uint32_t fp = (uint32_t)splitmix64(&x) & mask;
    return fp ? fp : 1;
}

static inline size_t hash1(uint64_t key, size_t nb) {
    uint64_t x = key;
    return (size_t)splitmix64(&x) & (nb - 1);
}

static inline size_t hash2(size_t h1, uint32_t fp, size_t nb) {
    return (h1 ^ ((size_t)fp * 0x5bd1e995u)) & (nb - 1);
}

static inline int bucket_has_fp(const ck_slot_t *b, uint32_t fp) {
    for (size_t i = 0; i < BUCKET_SIZE; i++) {
        if (b[i].fp == fp) return 1;
    }
    return 0;
}

static inline int bucket_insert_fp(ck_slot_t *b, uint32_t fp) {
    for (size_t i = 0; i < BUCKET_SIZE; i++) {
        if (b[i].fp == 0) {
            b[i].fp = fp;
            return 1;
        }
    }
    return 0;
}

static inline int bucket_delete_fp(ck_slot_t *b, uint32_t fp) {
    for (size_t i = 0; i < BUCKET_SIZE; i++) {
        if (b[i].fp == fp) {
            b[i].fp = 0;
            return 1;
        }
    }
    return 0;
}

static inline int stash_insert(cuckoo_filter_t *cf, uint64_t key, uint32_t fp) {
    if (cf->stash_size >= cf->stash_cap) return 0;
    cf->stash_keys[cf->stash_size] = key;
    cf->stash_fp[cf->stash_size] = fp;
    cf->stash_size++;
    cf->stats.stash_inserts++;
    cf->nitems++;
    return 1;
}

static inline int stash_query(cuckoo_filter_t *cf, uint64_t key) {
    for (size_t i = 0; i < cf->stash_size; i++) {
        if (cf->stash_keys[i] == key) {
            cf->stats.stash_hits++;
            return 1;
        }
    }
    return 0;
}

static inline int stash_delete(cuckoo_filter_t *cf, uint64_t key) {
    for (size_t i = 0; i < cf->stash_size; i++) {
        if (cf->stash_keys[i] == key) {
            cf->stash_keys[i] = cf->stash_keys[cf->stash_size - 1];
            cf->stash_fp[i] = cf->stash_fp[cf->stash_size - 1];
            cf->stash_size--;
            cf->nitems--;
            return 1;
        }
    }
    return 0;
}

static inline int try_place_no_kick(cuckoo_filter_t *cf, uint64_t key, uint32_t fp) {
    size_t i1 = hash1(key, cf->nbuckets);
    size_t i2 = hash2(i1, fp, cf->nbuckets);
    ck_slot_t *b1 = cf->table + i1 * BUCKET_SIZE;
    ck_slot_t *b2 = cf->table + i2 * BUCKET_SIZE;
    if (bucket_insert_fp(b1, fp)) return 1;
    if (bucket_insert_fp(b2, fp)) return 1;
    return 0;
}

int cuckoo_init(cuckoo_filter_t *cf, size_t nkeys_hint, int fp_bits) {
    if (!cf || fp_bits <= 0 || fp_bits > 32) return -1;
    memset(cf, 0, sizeof(*cf));
    cf->fp_bits = fp_bits;
    cf->fp_mask = (fp_bits == 32) ? 0xFFFFFFFFu : ((1u << fp_bits) - 1u);

    double load = 0.85;
    size_t nb = (size_t)ceil((double)nkeys_hint / (BUCKET_SIZE * load));
    size_t p = 1;
    while (p < nb) p <<= 1;
    nb = p;

    cf->nbuckets = nb;
    cf->bucket_size = BUCKET_SIZE;
    cf->nitems = 0;

    cf->table = (ck_slot_t*)calloc(nb * BUCKET_SIZE, sizeof(ck_slot_t));
    if (!cf->table) return -1;

    cf->stash_cap = STASH_CAP;
    cf->stash_size = 0;
    cf->stash_keys = (uint64_t*)malloc(sizeof(uint64_t) * cf->stash_cap);
    cf->stash_fp = (uint32_t*)malloc(sizeof(uint32_t) * cf->stash_cap);
    if (!cf->stash_keys || !cf->stash_fp) {
        free(cf->table);
        free(cf->stash_keys);
        free(cf->stash_fp);
        memset(cf, 0, sizeof(*cf));
        return -1;
    }

    memset(&cf->stats, 0, sizeof(cf->stats));
    return 0;
}

void cuckoo_free(cuckoo_filter_t *cf) {
    if (!cf) return;
    free(cf->table);
    free(cf->stash_keys);
    free(cf->stash_fp);
    memset(cf, 0, sizeof(*cf));
}

int cuckoo_query(cuckoo_filter_t *cf, uint64_t key) {
    uint32_t fp = fingerprint(key, cf->fp_mask);
    size_t i1 = hash1(key, cf->nbuckets);
    size_t i2 = hash2(i1, fp, cf->nbuckets);

    ck_slot_t *b1 = cf->table + i1 * BUCKET_SIZE;
    ck_slot_t *b2 = cf->table + i2 * BUCKET_SIZE;

    for (size_t i = 0; i < BUCKET_SIZE; i++) {
        if (b1[i].fp == fp || b2[i].fp == fp) return 1;
    }

    return stash_query(cf, key);
}

int cuckoo_insert(cuckoo_filter_t *cf, uint64_t key) {
    uint32_t fp = fingerprint(key, cf->fp_mask);
    size_t i1 = hash1(key, cf->nbuckets);
    size_t i2 = hash2(i1, fp, cf->nbuckets);

    ck_slot_t *b1 = cf->table + i1 * BUCKET_SIZE;
    ck_slot_t *b2 = cf->table + i2 * BUCKET_SIZE;

    cf->stats.insert_calls++;

    for (size_t i = 0; i < BUCKET_SIZE; i++) {
        cf->stats.total_probes += 2;
        if (b1[i].fp == 0) {
            b1[i].fp = fp;
            cf->nitems++;
            return 0;
        }
        if (b2[i].fp == 0) {
            b2[i].fp = fp;
            cf->nitems++;
            return 0;
        }
    }

    size_t idx = i1;
    uint32_t cur_fp = fp;

    for (size_t kick = 0; kick < MAX_KICKS; kick++) {
        ck_slot_t *b = cf->table + idx * BUCKET_SIZE;
        size_t victim = kick % BUCKET_SIZE;

        uint32_t tmp = b[victim].fp;
        b[victim].fp = cur_fp;
        cur_fp = tmp;

        cf->stats.total_kicks++;

        idx = hash2(idx, cur_fp, cf->nbuckets);
        b = cf->table + idx * BUCKET_SIZE;

        for (size_t j = 0; j < BUCKET_SIZE; j++) {
            cf->stats.total_probes += 1;
            if (b[j].fp == 0) {
                b[j].fp = cur_fp;
                cf->nitems++;
                return 0;
            }
        }
    }

    cf->stats.insert_fails++;
    if (stash_insert(cf, key, fp)) return 0;
    return -1;
}

int cuckoo_delete(cuckoo_filter_t *cf, uint64_t key) {
    uint32_t fp = fingerprint(key, cf->fp_mask);
    size_t i1 = hash1(key, cf->nbuckets);
    size_t i2 = hash2(i1, fp, cf->nbuckets);

    ck_slot_t *b1 = cf->table + i1 * BUCKET_SIZE;
    ck_slot_t *b2 = cf->table + i2 * BUCKET_SIZE;

    if (bucket_delete_fp(b1, fp) || bucket_delete_fp(b2, fp)) {
        cf->nitems--;

        for (size_t s = 0; s < cf->stash_size; s++) {
            uint64_t sk = cf->stash_keys[s];
            uint32_t sfp = cf->stash_fp[s];
            if (try_place_no_kick(cf, sk, sfp)) {
                cf->stash_keys[s] = cf->stash_keys[cf->stash_size - 1];
                cf->stash_fp[s] = cf->stash_fp[cf->stash_size - 1];
                cf->stash_size--;
                cf->nitems++;
                break;
            }
        }
        return 0;
    }

    if (stash_delete(cf, key)) return 0;
    return -1;
}

size_t cuckoo_bytes(const cuckoo_filter_t *cf) {
    return cf->nbuckets * cf->bucket_size * sizeof(ck_slot_t)
         + cf->stash_cap * (sizeof(uint64_t) + sizeof(uint32_t));
}
