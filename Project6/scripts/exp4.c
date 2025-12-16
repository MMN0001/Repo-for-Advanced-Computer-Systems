#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <inttypes.h>
#include <math.h>

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

typedef struct {
    int nthreads;
    int pin;
    int tid;

    const char *filter_name;
    int bits;

    double read_frac;
    double ins_frac;
    double del_frac;

    uint64_t t_warm_end;
    uint64_t t_end;

    volatile int *start_flag;

    const uint64_t *pos_keys;
    size_t n_pos;

    const uint64_t *ins_keys;
    size_t n_ins;

    cuckoo_filter_t *cf;
    quotient_filter_t *qf;

    pthread_mutex_t *wlock;

    uint64_t ops;
    uint64_t reads, ins, del;
    uint64_t ins_ok, del_ok;

    uint64_t rng;

    uint64_t *local_stack;
    size_t stack_cap;
    size_t stack_sz;
} worker_t;

static void pin_thread(int tid, int nthreads) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(tid % nthreads, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

static inline int do_cuckoo_insert(cuckoo_filter_t *cf, uint64_t key) {
    return cuckoo_insert(cf, key);
}
static inline int do_cuckoo_delete(cuckoo_filter_t *cf, uint64_t key) {
    return cuckoo_delete(cf, key);
}
static inline int do_cuckoo_query(const cuckoo_filter_t *cf, uint64_t key) {
    return cuckoo_query(cf, key);
}

static inline int do_qf_insert(quotient_filter_t *qf, uint64_t key) {
    return qf_insert(qf, key);
}
static inline int do_qf_delete(quotient_filter_t *qf, uint64_t key) {
    return qf_delete(qf, key);
}
static inline int do_qf_query(const quotient_filter_t *qf, uint64_t key) {
    return qf_query(qf, key);
}

static void *worker_main(void *arg) {
    worker_t *w = (worker_t*)arg;
    if (w->pin) pin_thread(w->tid, sysconf(_SC_NPROCESSORS_ONLN));

    while (!*w->start_flag) { }

    uint64_t ops = 0, reads = 0, ins = 0, del = 0, ins_ok = 0, del_ok = 0;

    for (;;) {
        uint64_t t = now_ns();
        if (t >= w->t_end) break;

        uint64_t r = splitmix64(&w->rng);
        double u = (double)(r >> 11) * (1.0 / 9007199254740992.0);

        if (u < w->read_frac) {
            size_t idx = (size_t)(splitmix64(&w->rng) % w->n_pos);
            uint64_t key = w->pos_keys[idx];
            if (strcmp(w->filter_name, "Cuckoo") == 0) (void)do_cuckoo_query(w->cf, key);
            else (void)do_qf_query(w->qf, key);
            reads++;
        } else if (u < w->read_frac + w->ins_frac) {
            size_t idx = (size_t)(splitmix64(&w->rng) % w->n_ins);
            uint64_t key = w->ins_keys[idx];

            pthread_mutex_lock(w->wlock);
            int rc = 0;
            if (strcmp(w->filter_name, "Cuckoo") == 0) rc = do_cuckoo_insert(w->cf, key);
            else rc = do_qf_insert(w->qf, key);
            if (rc == 0) {
                if (w->stack_sz < w->stack_cap) w->local_stack[w->stack_sz++] = key;
                ins_ok++;
            }
            pthread_mutex_unlock(w->wlock);
            ins++;
        } else {
            uint64_t key = 0;
            int have = 0;
            if (w->stack_sz > 0) {
                key = w->local_stack[--w->stack_sz];
                have = 1;
            } else {
                size_t idx = (size_t)(splitmix64(&w->rng) % w->n_ins);
                key = w->ins_keys[idx];
                have = 1;
            }

            if (have) {
                pthread_mutex_lock(w->wlock);
                int rc = 0;
                if (strcmp(w->filter_name, "Cuckoo") == 0) rc = do_cuckoo_delete(w->cf, key);
                else rc = do_qf_delete(w->qf, key);
                if (rc == 0) del_ok++;
                pthread_mutex_unlock(w->wlock);
            }
            del++;
        }

        ops++;
    }

    w->ops = ops;
    w->reads = reads;
    w->ins = ins;
    w->del = del;
    w->ins_ok = ins_ok;
    w->del_ok = del_ok;

    return NULL;
}

static size_t round_up_pow2(size_t x) {
    if (x <= 1) return 1;
    if ((x & (x - 1)) == 0) return x;
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

static void usage(const char *p) {
    fprintf(stderr,
        "Usage:\n"
        "  %s <filter:Cuckoo|Quotient> <bits> <nkeys> <load> <threads> <warm_ms> <run_ms> <workload:readmostly|balanced> [pin:0|1]\n"
        "Example:\n"
        "  %s Cuckoo 12 1000000 0.85 8 500 2000 balanced 1\n",
        p, p
    );
}

int main(int argc, char **argv) {
    if (argc < 9) { usage(argv[0]); return 1; }

    const char *filter = argv[1];
    int bits = atoi(argv[2]);
    size_t nkeys = (size_t)atoll(argv[3]);
    double load = atof(argv[4]);
    int nthreads = atoi(argv[5]);
    int warm_ms = atoi(argv[6]);
    int run_ms = atoi(argv[7]);
    const char *workload = argv[8];
    int pin = (argc >= 10) ? atoi(argv[9]) : 1;

    double read_frac = 0.95, ins_frac = 0.05, del_frac = 0.0;
    if (strcmp(workload, "balanced") == 0) {
        read_frac = 0.50;
        ins_frac = 0.25;
        del_frac = 0.25;
    } else if (strcmp(workload, "readmostly") == 0) {
        read_frac = 0.95;
        ins_frac = 0.05;
        del_frac = 0.00;
    } else {
        fprintf(stderr, "Unknown workload: %s\n", workload);
        return 1;
    }

    if (nthreads <= 0) return 1;
    if (!(strcmp(filter, "Cuckoo") == 0 || strcmp(filter, "Quotient") == 0)) {
        fprintf(stderr, "filter must be Cuckoo or Quotient\n");
        return 1;
    }

    size_t n_pos = nkeys;
    size_t n_ins = nkeys;
    uint64_t *pos_keys = (uint64_t*)malloc(sizeof(uint64_t) * n_pos);
    uint64_t *ins_keys = (uint64_t*)malloc(sizeof(uint64_t) * n_ins);
    if (!pos_keys || !ins_keys) { fprintf(stderr, "alloc failed\n"); return 1; }

    gen_keys(pos_keys, n_pos, 123456789ULL, 0);
    gen_keys(ins_keys, n_ins, 987654321ULL, 0x9e3779b97f4a7c15ULL);

    pthread_mutex_t wlock;
    pthread_mutex_init(&wlock, NULL);

    cuckoo_filter_t cf;
    quotient_filter_t qf;
    memset(&cf, 0, sizeof(cf));
    memset(&qf, 0, sizeof(qf));

    if (strcmp(filter, "Cuckoo") == 0) {
        if (cuckoo_init(&cf, nkeys, bits) != 0) { fprintf(stderr, "cuckoo_init failed\n"); return 1; }
        size_t cap = cf.nbuckets * cf.bucket_size;
        size_t target_items = (size_t)floor(load * (double)cap);
        if (target_items > n_pos) target_items = n_pos;
        for (size_t i = 0; i < target_items; i++) (void)cuckoo_insert(&cf, pos_keys[i]);
    } else {
        size_t slots_need = (size_t)ceil((double)nkeys / 0.95);
        size_t slots = round_up_pow2(slots_need);
        size_t qbits = 0;
        while ((1ULL << qbits) < slots) qbits++;
        if (qf_init(&qf, qbits, (size_t)bits) != 0) { fprintf(stderr, "qf_init failed\n"); return 1; }
        size_t cap = qf.nslots;
        size_t target_items = (size_t)floor(load * (double)cap);
        if (target_items > n_pos) target_items = n_pos;
        for (size_t i = 0; i < target_items; i++) (void)qf_insert(&qf, pos_keys[i]);
    }

    pthread_t *ths = (pthread_t*)malloc(sizeof(pthread_t) * (size_t)nthreads);
    worker_t *ws = (worker_t*)calloc((size_t)nthreads, sizeof(worker_t));
    if (!ths || !ws) { fprintf(stderr, "alloc failed\n"); return 1; }

    volatile int start_flag = 0;

    uint64_t t0 = now_ns();
    uint64_t t_warm_end = t0 + (uint64_t)warm_ms * 1000000ULL;
    uint64_t t_end = t_warm_end + (uint64_t)run_ms * 1000000ULL;

    for (int i = 0; i < nthreads; i++) {
        ws[i].nthreads = nthreads;
        ws[i].pin = pin;
        ws[i].tid = i;

        ws[i].filter_name = filter;
        ws[i].bits = bits;

        ws[i].read_frac = read_frac;
        ws[i].ins_frac = ins_frac;
        ws[i].del_frac = del_frac;

        ws[i].t_warm_end = t_warm_end;
        ws[i].t_end = t_end;
        ws[i].start_flag = &start_flag;

        ws[i].pos_keys = pos_keys;
        ws[i].n_pos = n_pos;

        ws[i].ins_keys = ins_keys;
        ws[i].n_ins = n_ins;

        ws[i].cf = &cf;
        ws[i].qf = &qf;

        ws[i].wlock = &wlock;

        ws[i].rng = 0xC0FFEEULL ^ (uint64_t)i * 0x9e3779b97f4a7c15ULL;

        ws[i].stack_cap = 1u << 20;
        ws[i].local_stack = (uint64_t*)malloc(sizeof(uint64_t) * ws[i].stack_cap);
        ws[i].stack_sz = 0;

        if (!ws[i].local_stack) { fprintf(stderr, "stack alloc failed\n"); return 1; }

        if (pthread_create(&ths[i], NULL, worker_main, &ws[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 1;
        }
    }

    start_flag = 1;

    for (int i = 0; i < nthreads; i++) pthread_join(ths[i], NULL);

    uint64_t total_ops = 0, total_reads = 0, total_ins = 0, total_del = 0, total_ins_ok = 0, total_del_ok = 0;
    for (int i = 0; i < nthreads; i++) {
        total_ops += ws[i].ops;
        total_reads += ws[i].reads;
        total_ins += ws[i].ins;
        total_del += ws[i].del;
        total_ins_ok += ws[i].ins_ok;
        total_del_ok += ws[i].del_ok;
        free(ws[i].local_stack);
    }

    double secs = (double)run_ms / 1000.0;
    double mops = secs > 0 ? (double)total_ops / secs / 1e6 : 0.0;

    printf("workload,filter,bits,threads,load,throughput_mops,total_ops,reads,ins,del,ins_ok,del_ok,pin\n");
    printf("%s,%s,%d,%d,%.2f,%.6f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d\n",
           workload, filter, bits, nthreads, load, mops,
           total_ops, total_reads, total_ins, total_del, total_ins_ok, total_del_ok, pin);

    free(ths);
    free(ws);

    if (strcmp(filter, "Cuckoo") == 0) cuckoo_free(&cf);
    else qf_free(&qf);

    pthread_mutex_destroy(&wlock);
    free(pos_keys);
    free(ins_keys);

    return 0;
}
