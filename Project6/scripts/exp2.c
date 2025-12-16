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

static size_t round_up_pow2(size_t x) {
    if (x <= 1) return 1;
    if ((x & (x - 1)) == 0) return x;
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

static void shuffle_u64(uint64_t *a, size_t n, uint64_t seed) {
    uint64_t s = seed;
    for (size_t i = n - 1; i > 0; i--) {
        uint64_t r = splitmix64(&s);
        size_t j = (size_t)(r % (i + 1));
        uint64_t tmp = a[i]; a[i] = a[j]; a[j] = tmp;
    }
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

static uint64_t percentile_u64(uint64_t *arr, size_t n, double p) {
    if (n == 0) return 0;
    size_t idx = (size_t)floor(p * (double)(n - 1));
    return arr[idx];
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


static const int NEG_SHARES[] = {0,10,20,30,40,50,60,70,80,90};

static void print_csv_header(void) {
    printf("filter,target_fpr,param_bits,neg_share,nqueries,secs,qps,mops,lat_p50_ns,lat_p95_ns,lat_p99_ns,hit_rate\n");
}


static void build_mixed_queries(uint64_t *out_q, size_t nqueries,
                                const uint64_t *pos, size_t npos,
                                const uint64_t *neg, size_t nneg,
                                int neg_share_percent,
                                uint64_t seed)
{
    size_t nneg_q = (size_t)((double)nqueries * (double)neg_share_percent / 100.0);
    if (nneg_q > nqueries) nneg_q = nqueries;
    size_t npos_q = nqueries - nneg_q;

    for (size_t i = 0; i < npos_q; i++) out_q[i] = pos[i % npos];
    for (size_t i = 0; i < nneg_q; i++) out_q[npos_q + i] = neg[i % nneg];

    shuffle_u64(out_q, nqueries, seed);
}

typedef int (*query_fn_t)(const void *filter, uint64_t key);

static void measure_mix(const char *name,
                        double target_fpr,
                        int param_bits,
                        int neg_share,
                        query_fn_t qfn,
                        const void *filter,
                        const uint64_t *queries,
                        size_t nqueries,
                        uint64_t *lat_ns_buf)
{
    volatile uint64_t sink = 0;
    size_t hits = 0;

    for (size_t i = 0; i < (nqueries < 10000 ? nqueries : 10000); i++) {
        sink += (uint64_t)qfn(filter, queries[i]);
    }

    uint64_t t0 = now_ns();
    for (size_t i = 0; i < nqueries; i++) {
        uint64_t a = now_ns();
        int h = qfn(filter, queries[i]);
        uint64_t b = now_ns();
        lat_ns_buf[i] = (b - a);
        hits += (size_t)h;
        sink += (uint64_t)h;
    }
    uint64_t t1 = now_ns();

    (void)sink;

    double secs = (double)(t1 - t0) * 1e-9;
    double qps  = (secs > 0) ? ((double)nqueries / secs) : 0.0;
    double mops = qps / 1e6;

    qsort(lat_ns_buf, nqueries, sizeof(uint64_t), cmp_u64);
    uint64_t p50 = percentile_u64(lat_ns_buf, nqueries, 0.50);
    uint64_t p95 = percentile_u64(lat_ns_buf, nqueries, 0.95);
    uint64_t p99 = percentile_u64(lat_ns_buf, nqueries, 0.99);

    double hit_rate = (nqueries > 0) ? ((double)hits / (double)nqueries) : 0.0;

    printf("%s,%.6f,%d,%d,%zu,%.6f,%.3f,%.3f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.6f\n",
           name, target_fpr, param_bits, neg_share, nqueries, secs, qps, mops, p50, p95, p99, hit_rate);
}


static int bb_query_adapter(const void *f, uint64_t k) {
    const blocked_bloom_t *bf = (const blocked_bloom_t*)f;
    return blocked_bloom_query(bf, k);
}

static int xor_query_adapter(const void *f, uint64_t k) {
    const xor_filter_t *xf = (const xor_filter_t*)f;
    return xor_query(xf, k);
}

static int ck_query_adapter(const void *f, uint64_t k) {
  
    cuckoo_filter_t *cf = (cuckoo_filter_t*)f;
    return cuckoo_query(cf, k);
}

static int qf_query_adapter(const void *f, uint64_t k) {
    const quotient_filter_t *qf = (const quotient_filter_t*)f;
    return qf_query(qf, k);
}


int main(int argc, char **argv) {
  
    size_t n = (argc >= 2) ? (size_t)atoll(argv[1]) : 1000000ULL;
    size_t nqueries = (argc >= 3) ? (size_t)atoll(argv[2]) : 1000000ULL;

    // pos keys for building
    uint64_t *pos = (uint64_t*)malloc(sizeof(uint64_t) * n);
    // neg keys pool for negative queries
    uint64_t *neg = (uint64_t*)malloc(sizeof(uint64_t) * nqueries);
    // mixed query array
    uint64_t *queries = (uint64_t*)malloc(sizeof(uint64_t) * nqueries);
    // latency buffer
    uint64_t *lat = (uint64_t*)malloc(sizeof(uint64_t) * nqueries);

    if (!pos || !neg || !queries || !lat) {
        fprintf(stderr, "alloc failed\n");
        free(pos); free(neg); free(queries); free(lat);
        return 1;
    }

    gen_keys(pos, n, 123456789ULL, 0);
    gen_keys(neg, nqueries, 987654321ULL, 0x9e3779b97f4a7c15ULL);

    print_csv_header();

    for (size_t ci = 0; ci < sizeof(CFGS)/sizeof(CFGS[0]); ci++) {
        double target_fpr = CFGS[ci].target_fpr;
        int bits = CFGS[ci].bits;

        // Blocked Bloom
        blocked_bloom_t bf;
        if (blocked_bloom_init(&bf, n, target_fpr) != 0) {
            fprintf(stderr, "[BB] init failed\n");
            continue;
        }
        for (size_t i = 0; i < n; i++) blocked_bloom_insert(&bf, pos[i]);

        // XOR
        xor_filter_t xf;
        if (xor_build(&xf, pos, (uint32_t)n, (uint32_t)bits, 1) != 0) {
            fprintf(stderr, "[XOR] build failed\n");
            blocked_bloom_free(&bf);
            continue;
        }

        // Cuckoo 
        cuckoo_filter_t cf;
        if (cuckoo_init(&cf, n, bits) != 0) {
            fprintf(stderr, "[Cuckoo] init failed\n");
            xor_free(&xf);
            blocked_bloom_free(&bf);
            continue;
        }
        for (size_t i = 0; i < n; i++) (void)cuckoo_insert(&cf, pos[i]);

        // QF 
        const double qf_load = 0.70;
        size_t slots_need = (size_t)ceil((double)n / qf_load);
        size_t slots = round_up_pow2(slots_need);
        size_t qbits = 0;
        while ((1ULL << qbits) < slots) qbits++;

        quotient_filter_t qf;
        if (qf_init(&qf, qbits, (size_t)bits) != 0) {
            fprintf(stderr, "[QF] init failed\n");
            cuckoo_free(&cf);
            xor_free(&xf);
            blocked_bloom_free(&bf);
            continue;
        }
        for (size_t i = 0; i < n; i++) (void)qf_insert(&qf, pos[i]);

        for (size_t si = 0; si < sizeof(NEG_SHARES)/sizeof(NEG_SHARES[0]); si++) {
            int neg_share = NEG_SHARES[si];

            build_mixed_queries(queries, nqueries, pos, n, neg, nqueries, neg_share, 0xBADC0FFEEULL + (uint64_t)neg_share);

            measure_mix("BlockedBloom", target_fpr, -1,  neg_share, bb_query_adapter,  &bf, queries, nqueries, lat);
            measure_mix("XOR",         target_fpr, bits, neg_share, xor_query_adapter, &xf, queries, nqueries, lat);
            measure_mix("Cuckoo",      target_fpr, bits, neg_share, ck_query_adapter,  &cf, queries, nqueries, lat);
            measure_mix("Quotient",    target_fpr, bits, neg_share, qf_query_adapter,  &qf, queries, nqueries, lat);
        }

        qf_free(&qf);
        cuckoo_free(&cf);
        xor_free(&xf);
        blocked_bloom_free(&bf);
    }

    free(pos);
    free(neg);
    free(queries);
    free(lat);
    return 0;
}
