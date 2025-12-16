#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

#include "ck.h"
#include "qf.h"

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

static void shuffle_u64(uint64_t *a, size_t n, uint64_t seed) {
    uint64_t s = seed;
    for (size_t i = n; i > 1; i--) {
        size_t j = (size_t)(splitmix64(&s) % i);
        uint64_t tmp = a[i - 1];
        a[i - 1] = a[j];
        a[j] = tmp;
    }
}

static inline double ns_to_sec(uint64_t ns) { return (double)ns / 1e9; }

static inline int qf_slot_empty(const qf_slot_t *s) {
    return (s->rem == 0) && (s->continuation == 0) && (s->shifted == 0) && (s->occupied == 0);
}

static size_t qf_collect_cluster_lengths(const quotient_filter_t *qf, uint32_t **out_lens) {
    size_t n = qf->nslots;
    uint32_t *lens = (uint32_t*)malloc(sizeof(uint32_t) * n);
    if (!lens) return 0;

    size_t cnt = 0;
    for (size_t i = 0; i < n; i++) {
        const qf_slot_t *s = &qf->slots[i];
        if (qf_slot_empty(s)) continue;
        if (s->shifted) continue;

        uint32_t len = 1;
        size_t j = (i + 1) & (n - 1);
        while (j != i) {
            const qf_slot_t *t = &qf->slots[j];
            if (qf_slot_empty(t)) break;
            if (!(t->shifted || t->continuation)) break;
            len++;
            j = (j + 1) & (n - 1);
        }
        lens[cnt++] = len;
    }

    *out_lens = lens;
    return cnt;
}

static int cmp_u32(const void *a, const void *b) {
    uint32_t x = *(const uint32_t*)a;
    uint32_t y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}

static uint32_t quantile_u32(uint32_t *a, size_t n, double q) {
    if (n == 0) return 0;
    size_t idx = (size_t)floor(q * (double)(n - 1));
    return a[idx];
}

static const double LOADS[] = {
    0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95
};

static const int BITS[] = { 8, 12, 16 };

static void run_cuckoo_sweep(size_t n_max, const uint64_t *keys, int fp_bits) {
    cuckoo_filter_t cf;
    if (cuckoo_init(&cf, n_max, fp_bits) != 0) {
        fprintf(stderr, "[Cuckoo] init failed (n_max=%zu fp_bits=%d)\n", n_max, fp_bits);
        return;
    }

    for (size_t li = 0; li < sizeof(LOADS)/sizeof(LOADS[0]); li++) {
        double target_load = LOADS[li];

        cuckoo_free(&cf);
        if (cuckoo_init(&cf, n_max, fp_bits) != 0) {
            fprintf(stderr, "[Cuckoo] init failed in loop\n");
            return;
        }

        size_t capacity_slots = cuckoo_capacity_slots(&cf);
        size_t target_items = (size_t)floor(target_load * (double)capacity_slots);
        if (target_items > n_max) target_items = n_max;

        uint64_t t0 = now_ns();
        size_t attempts = 0;
        size_t hard_fails = 0;
        for (size_t i = 0; i < target_items; i++) {
            attempts++;
            if (cuckoo_insert(&cf, keys[i]) != 0) hard_fails++;
        }
        uint64_t t1 = now_ns();

        size_t inserted = cf.nitems;

        double insert_sec = ns_to_sec(t1 - t0);
        double insert_mops = insert_sec > 0 ? (double)attempts / insert_sec / 1e6 : 0.0;

        double fail_rate = attempts ? (double)hard_fails / (double)attempts : 0.0;
        double avg_probe = cf.stats.insert_calls ? (double)cf.stats.total_probes / (double)cf.stats.insert_calls : 0.0;

        uint64_t *del = (uint64_t*)malloc(sizeof(uint64_t) * target_items);
        if (!del) { fprintf(stderr, "alloc failed\n"); return; }
        memcpy(del, keys, sizeof(uint64_t) * target_items);
        shuffle_u64(del, target_items, 0xBADC0FFEEULL ^ (uint64_t)target_items);

        uint64_t t2 = now_ns();
        size_t del_ops = 0;
        for (size_t i = 0; i < target_items; i++) {
            (void)cuckoo_delete(&cf, del[i]);
            del_ops++;
        }
        uint64_t t3 = now_ns();
        free(del);

        double delete_sec = ns_to_sec(t3 - t2);
        double delete_mops = delete_sec > 0 ? (double)del_ops / delete_sec / 1e6 : 0.0;

        printf("Cuckoo,%d,%.2f,%zu,%zu,%zu,%.6f,%.6f,%zu,%zu,%.6f,%.6f,,,,\n",
               fp_bits,
               target_load,
               capacity_slots,
               attempts,
               inserted,
               insert_mops,
               fail_rate,
               cf.stats.total_kicks,
               cf.stats.stash_inserts,
               avg_probe,
               delete_mops);
    }

    cuckoo_free(&cf);
}

static size_t round_up_pow2(size_t x) {
    if (x <= 1) return 1;
    if ((x & (x - 1)) == 0) return x;
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

static void run_qf_sweep(size_t n_max, const uint64_t *keys, int rbits) {
    size_t slots_need = (size_t)ceil((double)n_max / 0.95);
    size_t slots = round_up_pow2(slots_need);
    size_t qbits = 0;
    while ((1ULL << qbits) < slots) qbits++;

    for (size_t li = 0; li < sizeof(LOADS)/sizeof(LOADS[0]); li++) {
        double target_load = LOADS[li];
        quotient_filter_t qf;
        if (qf_init(&qf, qbits, (size_t)rbits) != 0) {
            fprintf(stderr, "[QF] init failed (qbits=%zu rbits=%d)\n", qbits, rbits);
            return;
        }

        size_t target_items = (size_t)floor(target_load * (double)qf.nslots);
        if (target_items > n_max) target_items = n_max;

        uint64_t t0 = now_ns();
        size_t attempts = 0;
        size_t fails = 0;
        for (size_t i = 0; i < target_items; i++) {
            attempts++;
            if (qf_insert(&qf, keys[i]) != 0) fails++;
        }
        uint64_t t1 = now_ns();

        double insert_sec = ns_to_sec(t1 - t0);
        double insert_mops = insert_sec > 0 ? (double)attempts / insert_sec / 1e6 : 0.0;

        uint32_t *lens = NULL;
        size_t cnt = qf_collect_cluster_lengths(&qf, &lens);

        uint32_t p50 = 0, p95 = 0, p99 = 0, mx = 0;
        if (cnt > 0) {
            qsort(lens, cnt, sizeof(uint32_t), cmp_u32);
            p50 = quantile_u32(lens, cnt, 0.50);
            p95 = quantile_u32(lens, cnt, 0.95);
            p99 = quantile_u32(lens, cnt, 0.99);
            mx  = lens[cnt - 1];
        }
        free(lens);

        uint64_t *del = (uint64_t*)malloc(sizeof(uint64_t) * target_items);
        if (!del) { fprintf(stderr, "alloc failed\n"); qf_free(&qf); return; }
        memcpy(del, keys, sizeof(uint64_t) * target_items);
        shuffle_u64(del, target_items, 0x1234ULL ^ (uint64_t)target_items);

        uint64_t t2 = now_ns();
        size_t del_ops = 0;
        for (size_t i = 0; i < target_items; i++) {
            (void)qf_delete(&qf, del[i]);
            del_ops++;
        }
        uint64_t t3 = now_ns();
        free(del);

        double delete_sec = ns_to_sec(t3 - t2);
        double delete_mops = delete_sec > 0 ? (double)del_ops / delete_sec / 1e6 : 0.0;

        printf("Quotient,%d,%.2f,%zu,%zu,%zu,%.6f,%.6f,,,,%.6f,%u,%u,%u,%u\n",
               rbits,
               target_load,
               qf.nslots,
               attempts,
               qf.nitems,
               insert_mops,
               attempts ? (double)fails / (double)attempts : 0.0,
               delete_mops,
               p50, p95, p99, mx);

        qf_free(&qf);
    }
}

static void print_header(void) {
    printf("filter,param_bits,load,capacity_slots,insert_attempts,inserted,insert_mops,insert_fail_rate,"
           "cuckoo_total_kicks,cuckoo_stash_inserts,cuckoo_avg_probe,delete_mops,"
           "qf_cluster_p50,qf_cluster_p95,qf_cluster_p99,qf_cluster_max\n");
}

int main(int argc, char **argv) {
    size_t n_max = (argc >= 2) ? (size_t)atoll(argv[1]) : 1000000ULL;

    uint64_t *keys = (uint64_t*)malloc(sizeof(uint64_t) * n_max);
    if (!keys) {
        fprintf(stderr, "alloc failed\n");
        return 1;
    }
    gen_keys(keys, n_max, 123456789ULL, 0);

    print_header();

    for (size_t bi = 0; bi < sizeof(BITS)/sizeof(BITS[0]); bi++) {
        int bits = BITS[bi];
        run_cuckoo_sweep(n_max, keys, bits);
        run_qf_sweep(n_max, keys, bits);
    }

    free(keys);
    return 0;
}
