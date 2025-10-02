#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

static double now_ns(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec*1e9 + (double)ts.tv_nsec;
}

// stride: step size in bytes
// bytes: total memory size
// return: pointer to the index array
static size_t* make_ring(size_t bytes, size_t stride){
    size_t elems = bytes / sizeof(size_t);
    if (elems < 2) elems = 2;
    // Convert stride to element step
    size_t step = (stride < sizeof(size_t)) ? 1 : (stride / sizeof(size_t));
    if (step == 0) step = 1;

    size_t *p = NULL;
    if (posix_memalign((void**)&p, 64, elems*sizeof(size_t))) return NULL;

    // Initialize the ring: i -> (i+step) % elems
    for(size_t i=0;i<elems;i++){
        p[i] = (i + step) % elems;
    }
    // Simple shuffle of starting points to avoid regularity
    for(size_t i=0;i<elems;i++){
        size_t j = (i*2654435761u) % elems;
        size_t tmp = p[i]; p[i] = p[j]; p[j] = tmp;
    }
    return p;
}

// Pin the process to core 0
static void pin_core0(void){
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

int main(int argc, char** argv){
    // Default parameters: size=1MB, stride=64B, iterations=50M
    size_t bytes = 1<<20;      // 1 MiB
    size_t stride = 64;        // 64B
    size_t iters  = 50ull*1000*1000;

    // Parse command line options: --size 16K/256K/2M/64M,
    // --stride 64/256/1024, --iters N
    for(int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--size") && i+1<argc){
            char *s = argv[++i]; char u=s[strlen(s)-1];
            size_t v = strtoull(s, NULL, 10);
            if (u=='K'||u=='k') v <<= 10;
            else if (u=='M'||u=='m') v <<= 20;
            else if (u=='G'||u=='g') v <<= 30;
            bytes = v;
        } else if (!strcmp(argv[i],"--stride") && i+1<argc){
            stride = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i],"--iters") && i+1<argc){
            iters = strtoull(argv[++i], NULL, 10);
        }
    }

    pin_core0();

    size_t *ring = make_ring(bytes, stride);
    if (!ring){ fprintf(stderr,"alloc failed\n"); return 1; }

    // Warm-up loop
    volatile size_t idx = 0;
    for (size_t i=0;i<100000;i++) idx = ring[idx];

    double t0 = now_ns();
    for (size_t i=0;i<iters;i++) idx = ring[idx];
    double t1 = now_ns();

    double avg_ns = (t1 - t0) / (double)iters;
    // Print: size, stride, average time per access
    printf("size=%zuB stride=%zub iters=%zu avg=%.3f ns\n", bytes, stride, iters, avg_ns);

    // Prevent compiler optimization
    fprintf(stderr,"sink=%zu\n", (size_t)idx);
    free(ring);
    return 0;
}
