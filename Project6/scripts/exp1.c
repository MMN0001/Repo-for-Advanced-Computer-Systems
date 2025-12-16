#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>

#include "bb.h"
#include "ck.h"
#include "qf.h"
#include "xor.h"


static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static void gen_keys(uint64_t *out, size_t n, uint64_t seed, uint64_t add) {
    uint64_t s = seed;
    for (size_t i = 0; i < n; i++) out[i] = splitmix64(&s) + add;
}

static double bpe_from_bytes(size_t bytes, size_t n_inserted) {
    if (n_inserted == 0) return 0.0;
    return (double)bytes * 8.0 / (double)n_inserted;
}

static size_t round_up_pow2(size_t x) {
    if (x <= 1) return 1;
    if ((x & (x - 1)) == 0) return x;
    size_t p = 1;
    while (p < x) {
        if (p > (SIZE_MAX >> 1)) return 0;
        p <<= 1;
    }
    return p;
}


typedef struct {
    double target_fpr;  
    int bits;           
} cfg_t;

static const cfg_t CFGS[] = {
    {0.05,  8},
    {0.01, 12},
    {0.001,16},
};

static void print_csv_header(void) {
    printf("filter,target_fpr,param_bits,n_inserted,bytes,bpe,neg_queries,fp_count,achieved_fpr,build_ms\n");
}


static void run_blocked_bloom(size_t n, size_t qneg,
                              const uint64_t *pos, const uint64_t *neg,
                              double target_fpr)
{
    blocked_bloom_t bf;

    uint64_t t0 = now_ns();
    if (blocked_bloom_init(&bf, n, target_fpr) != 0) {
        fprintf(stderr, "[BlockedBloom] init failed\n");
        return;
    }
    for (size_t i = 0; i < n; i++) blocked_bloom_insert(&bf, pos[i]);
    uint64_t t1 = now_ns();

    size_t fp = 0;
    for (size_t i = 0; i < qneg; i++) if (blocked_bloom_query(&bf, neg[i])) fp++;

    size_t bytes = blocked_bloom_bytes(&bf);
    double bpe = bpe_from_bytes(bytes, n);
    double fpr = (double)fp / (double)qneg;
    double build_ms = (double)(t1 - t0) / 1e6;

    printf("BlockedBloom,%.6f,%d,%zu,%zu,%.6f,%zu,%zu,%.6f,%.3f\n",
           target_fpr, -1, n, bytes, bpe, qneg, fp, fpr, build_ms);

    blocked_bloom_free(&bf);
}

static void run_xor(size_t n, size_t qneg,
                    const uint64_t *pos, const uint64_t *neg,
                    double target_fpr, int fp_bits)
{
    xor_filter_t xf;

    uint64_t t0 = now_ns();
    if (xor_build(&xf, pos, (uint32_t)n, (uint32_t)fp_bits, 1) != 0) {
        fprintf(stderr, "[XOR] build failed\n");
        return;
    }
    uint64_t t1 = now_ns();

    size_t fn = 0;
    for (size_t i = 0; i < n; i++) if (!xor_query(&xf, pos[i])) fn++;
    if (fn) fprintf(stderr, "[XOR] FN=%zu (should be 0)\n", fn);

    size_t fp = 0;
    for (size_t i = 0; i < qneg; i++) if (xor_query(&xf, neg[i])) fp++;

    size_t bytes = xor_bytes(&xf);
    double bpe = bpe_from_bytes(bytes, n);
    double fpr = (double)fp / (double)qneg;
    double build_ms = (double)(t1 - t0) / 1e6;

    printf("XOR,%.6f,%d,%zu,%zu,%.6f,%zu,%zu,%.6f,%.3f\n",
           target_fpr, fp_bits, n, bytes, bpe, qneg, fp, fpr, build_ms);

    xor_free(&xf);
}

static void run_cuckoo(size_t n, size_t qneg,
                       const uint64_t *pos, const uint64_t *neg,
                       double target_fpr,
                       int fp_bits)
{
    cuckoo_filter_t cf;

    uint64_t t0 = now_ns();
    if (cuckoo_init(&cf, n, fp_bits) != 0) {
        fprintf(stderr, "[Cuckoo] init failed\n");
        return;
    }

    size_t fail = 0;
    for (size_t i = 0; i < n; i++) {
        if (cuckoo_insert(&cf, pos[i]) != 0) fail++;
    }
    uint64_t t1 = now_ns();

    size_t fn = 0;
    for (size_t i = 0; i < n; i++) if (!cuckoo_query(&cf, pos[i])) fn++;
    if (fn) fprintf(stderr, "[Cuckoo] FN=%zu (insert_fail=%zu)\n", fn, fail);

    size_t fp = 0;
    for (size_t i = 0; i < qneg; i++) if (cuckoo_query(&cf, neg[i])) fp++;

    size_t inserted = cf.nitems; 
    size_t bytes = cuckoo_bytes(&cf);
    double bpe = bpe_from_bytes(bytes, inserted);
    double fpr = (double)fp / (double)qneg;
    double build_ms = (double)(t1 - t0) / 1e6;

    printf("Cuckoo,%.6f,%d,%zu,%zu,%.6f,%zu,%zu,%.6f,%.3f\n",
       target_fpr, fp_bits, inserted, bytes, bpe, qneg, fp, fpr, build_ms);


    cuckoo_free(&cf);
}

static void run_qf(size_t n, size_t qneg,
                   const uint64_t *pos, const uint64_t *neg,
                   double target_fpr, int rbits)
{
    const double target_load = 0.85;
    size_t slots_need = (size_t)ceil((double)n / target_load);
    size_t slots = round_up_pow2(slots_need);
    if (slots == 0) {
        fprintf(stderr, "[QF] slots overflow\n");
        return;
    }

    size_t qbits = 0;
    while ((1ULL << qbits) < slots) qbits++;

    quotient_filter_t qf;
    uint64_t t0 = now_ns();
    if (qf_init(&qf, qbits, (size_t)rbits) != 0) {
        fprintf(stderr, "[QF] init failed\n");
        return;
    }

    size_t fail = 0;
    for (size_t i = 0; i < n; i++) {
        if (qf_insert(&qf, pos[i]) != 0) fail++;
    }
    uint64_t t1 = now_ns();

    size_t fn = 0;
    for (size_t i = 0; i < n; i++) if (!qf_query(&qf, pos[i])) fn++;
    if (fn) fprintf(stderr, "[QF] FN=%zu (insert_fail=%zu)\n", fn, fail);

    size_t fp = 0;
    for (size_t i = 0; i < qneg; i++) if (qf_query(&qf, neg[i])) fp++;

    size_t inserted = qf.nitems;
    size_t bytes = qf_bytes(&qf);
    double bpe = bpe_from_bytes(bytes, inserted);
    double fpr = (double)fp / (double)qneg;
    double build_ms = (double)(t1 - t0) / 1e6;

    printf("Quotient,%.6f,%d,%zu,%zu,%.6f,%zu,%zu,%.6f,%.3f\n",
           target_fpr, rbits, inserted, bytes, bpe, qneg, fp, fpr, build_ms);

    qf_free(&qf);
}


static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [n] [qneg] [pos_seed] [neg_seed]\n"
        "  n        default 1000000\n"
        "  qneg     default 1000000\n"
        "  pos_seed default 123456789\n"
        "  neg_seed default 987654321\n",
        prog);
}

int main(int argc, char **argv) {
    size_t n    = (argc >= 2) ? (size_t)atoll(argv[1]) : 1000000ULL;
    size_t qneg = (argc >= 3) ? (size_t)atoll(argv[2]) : 1000000ULL;
    uint64_t pos_seed = (argc >= 4) ? (uint64_t)strtoull(argv[3], NULL, 10) : 123456789ULL;
    uint64_t neg_seed = (argc >= 5) ? (uint64_t)strtoull(argv[4], NULL, 10) : 987654321ULL;

    if (n == 0 || qneg == 0) {
        usage(argv[0]);
        return 1;
    }

    uint64_t *pos = (uint64_t*)malloc(sizeof(uint64_t) * n);
    uint64_t *neg = (uint64_t*)malloc(sizeof(uint64_t) * qneg);
    if (!pos || !neg) {
        fprintf(stderr, "alloc failed\n");
        free(pos); free(neg);
        return 1;
    }

    gen_keys(pos, n,    pos_seed, 0);
    gen_keys(neg, qneg, neg_seed, 0x9e3779b97f4a7c15ULL);

    print_csv_header();

    for (size_t i = 0; i < sizeof(CFGS)/sizeof(CFGS[0]); i++) {
        double target = CFGS[i].target_fpr;
        int bits = CFGS[i].bits;

        run_blocked_bloom(n, qneg, pos, neg, target);
        run_xor(n, qneg, pos, neg, target, bits);
        run_cuckoo(n, qneg, pos, neg, target, bits);
        run_qf(n, qneg, pos, neg, target, bits);
    }

    free(pos);
    free(neg);
    return 0;
}
