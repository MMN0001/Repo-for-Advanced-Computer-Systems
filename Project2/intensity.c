#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>

static double now_ns(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec*1e9 + (double)ts.tv_nsec;
}

static size_t* make_ring(size_t bytes, size_t stride){
    size_t elems = bytes / sizeof(size_t);
    if (elems < 2) elems = 2;
    size_t step = (stride < sizeof(size_t)) ? 1 : (stride / sizeof(size_t));
    if (step == 0) step = 1;

    size_t *p = NULL;
    if (posix_memalign((void**)&p, 64, elems*sizeof(size_t))) return NULL;

    for (size_t i=0;i<elems;i++){
        p[i] = (i + step) % elems;
    }
    for (size_t i=0;i<elems;i++){
        size_t j = (i*2654435761u) % elems;
        size_t tmp = p[i]; p[i] = p[j]; p[j] = tmp;
    }
    return p;
}

static void pin_to_core(int core_id){
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(core_id, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

typedef struct {
    size_t *ring;       
    size_t  iters;       
    int     core_id;     
    volatile size_t sink;
} worker_t;

static void* worker_fn(void* arg){
    worker_t* w = (worker_t*)arg;
    if (w->core_id >= 0) pin_to_core(w->core_id);

  
    volatile size_t idx = 0;
    for (size_t k=0;k<10000;k++) idx = w->ring[idx];
    w->sink += idx;

 
    for (size_t i=0; i<w->iters; ++i){
        idx = w->ring[idx];
        w->sink += idx;
    }
    return NULL;
}

int main(int argc, char** argv){
    size_t bytes_per_thread = 32ull<<20;
    size_t stride = 64;
    size_t iters  = 10ull*1000*1000;
    size_t threads = 1;
    int    pin = 0;

    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--size") && i+1<argc){
            char *s = argv[++i]; char u=s[strlen(s)-1];
            size_t v = strtoull(s, NULL, 10);
            if (u=='K'||u=='k') v <<= 10;
            else if (u=='M'||u=='m') v <<= 20;
            else if (u=='G'||u=='g') v <<= 30;
            bytes_per_thread = v;
        } else if (!strcmp(argv[i],"--stride") && i+1<argc){
            stride = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i],"--iters") && i+1<argc){
            iters = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i],"--threads") && i+1<argc){  
            threads = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i],"--no-pin")){
            pin = 0;
        } else if (!strcmp(argv[i],"--pin")){
            pin = 1;
        }
    }

    size_t **rings = (size_t**)calloc(threads, sizeof(size_t*));
    if (!rings){ fprintf(stderr,"alloc ring ptrs failed\n"); return 1; }
    for (size_t t=0; t<threads; ++t){
        rings[t] = make_ring(bytes_per_thread, stride);
        if (!rings[t]){ fprintf(stderr,"make_ring failed at t=%zu\n", t); return 1; }
    }

    pthread_t *tids = (pthread_t*)calloc(threads, sizeof(pthread_t));
    worker_t  *wrk  = (worker_t*)calloc(threads, sizeof(worker_t));
    if (!tids || !wrk){ fprintf(stderr,"alloc workers failed\n"); return 1; }

    double t0 = now_ns();
    for (size_t t=0; t<threads; ++t){
        wrk[t].ring    = rings[t];   
        wrk[t].iters   = iters;
        wrk[t].sink    = 0;
        wrk[t].core_id = pin ? (int)t : -1;
        if (pthread_create(&tids[t], NULL, worker_fn, &wrk[t])){
            fprintf(stderr,"pthread_create failed\n"); return 1;
        }
    }
    for (size_t t=0; t<threads; ++t) pthread_join(tids[t], NULL);
    double t1 = now_ns();

    uint64_t total_accesses = (uint64_t)threads * (uint64_t)iters;
    double dur_ns = t1 - t0;
    double avg_ns = dur_ns / (double)total_accesses;

    size_t line_bytes = (stride >= 64) ? 64 : stride;
    if (line_bytes == 0) line_bytes = 64;
    double bytes_total = (double)total_accesses * (double)line_bytes;
    double bw_GBps = (bytes_total / dur_ns) * 1e9 / (1024.0*1024.0*1024.0);

    uint64_t intensity = (uint64_t)threads;
    printf("%zu,%zu,%" PRIu64 ",%zu,%d,%" PRIu64 ",%.3f,%.2f\n",
           bytes_per_thread, stride, (uint64_t)iters,
           threads, 1, intensity, avg_ns, bw_GBps);

    size_t sum_sink = 0;
    for (size_t t=0; t<threads; ++t) sum_sink += (size_t)wrk[t].sink;
    fprintf(stderr,"sink=%zu\n", sum_sink);

    for (size_t t=0; t<threads; ++t) free(rings[t]);
    free(rings); free(tids); free(wrk);
    return 0;
}
