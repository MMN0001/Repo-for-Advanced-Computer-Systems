#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <inttypes.h> 

static double now_ns(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec*1e9 + (double)ts.tv_nsec;
}

static void pin_core0(void){
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

// "100R", "100W", "70/30", "50/50"
static void parse_mix(const char* s, int* reads_per10, int* writes_per10){
    *reads_per10 = 10; *writes_per10 = 0; 
    if (!s) return;

    if (!strcasecmp(s,"100R")) { *reads_per10 = 10; *writes_per10 = 0;  return; }
    if (!strcasecmp(s,"100W")) { *reads_per10 = 0;  *writes_per10 = 10; return; }
    if (!strcasecmp(s,"50/50")){ *reads_per10 = 5;  *writes_per10 = 5;  return; }
    if (!strcasecmp(s,"70/30")){ *reads_per10 = 7;  *writes_per10 = 3;  return; }

    int a=0,b=0; char c=0;
    if (sscanf(s, "%d %c %d", &a, &c, &b) == 3 && (c=='/'||c==':')) {
        int sum = a + b; if (sum <= 0) sum = 10;
        *reads_per10  = (int)((10.0 * a) / (double)sum + 0.5);
        *writes_per10 = 10 - *reads_per10;
        return;
    }
    fprintf(stderr, "WARN: unknown --mix '%s', fallback to 100R\n", s);
}

static double bytes_touched_est(uint64_t accesses, size_t line_bytes){
    return (double)accesses * (double)line_bytes;
}

int main(int argc, char** argv){
    size_t bytes  = 64ull<<20;     
    size_t stride = 64;            
    uint64_t iters = 50ull*1000*1000; 
    const char* mix_str = "100R";

    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--size") && i+1<argc){
            char *s = argv[++i]; char u = s[strlen(s)-1];
            size_t v = strtoull(s, NULL, 10);
            if (u=='K'||u=='k') v <<= 10;
            else if (u=='M'||u=='m') v <<= 20;
            else if (u=='G'||u=='g') v <<= 30;
            bytes = v;
        } else if (!strcmp(argv[i],"--stride") && i+1<argc){
            stride = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i],"--iters") && i+1<argc){
            iters = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i],"--mix") && i+1<argc){
            mix_str = argv[++i];
        }
    }

    pin_core0();

    size_t elem_size = sizeof(uint64_t);
    size_t elems = bytes / elem_size;
    if (elems < 1024) elems = 1024; 
    uint64_t* arr = NULL;
    if (posix_memalign((void**)&arr, 64, elems * elem_size)){
        fprintf(stderr, "alloc failed: %s\n", strerror(errno));
        return 1;
    }

    for (size_t i=0;i<elems;i++) arr[i] = (uint64_t)i * 1315423911ull;

    size_t step = stride / elem_size; if (step == 0) step = 1;

    int r10=10, w10=0;
    parse_mix(mix_str, &r10, &w10);
    int win = 10;

    volatile uint64_t sink = 0;
    size_t idx = 0;
    uint64_t accesses = 0;


    for (size_t k=0;k<100000;k++){ sink += arr[(idx += step) % elems]; }

    double t0 = now_ns();

    while (accesses < iters){
        for (int i=0; i<r10 && accesses<iters; i++, accesses++){
            idx += step; if (idx >= elems) idx -= elems;
            sink += arr[idx];
        }
        for (int i=0; i<w10 && accesses<iters; i++, accesses++){
            idx += step; if (idx >= elems) idx -= elems;
            arr[idx] = arr[idx] + 1;
        }
    }

    double t1 = now_ns();
    double dur_ns = t1 - t0;
    double avg_ns = dur_ns / (double)iters;

    size_t line_bytes = (stride >= 64) ? 64 : stride;
    double bytes_total = bytes_touched_est(iters, line_bytes);
    double bw_GBps = (bytes_total / dur_ns) * 1e9 / (1024.0*1024.0*1024.0);

    printf("%zu,%zu,%" PRIu64 ",%s,%.3f,%.2f\n",
       bytes, stride, iters, mix_str, avg_ns, bw_GBps);

    free((void*)arr);
    return 0;
}
