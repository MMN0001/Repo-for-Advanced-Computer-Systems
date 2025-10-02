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


static size_t* make_ring(size_t bytes, size_t stride){
    size_t elems = bytes / sizeof(size_t);
    if (elems < 2) elems = 2;
    size_t step = (stride < sizeof(size_t)) ? 1 : (stride / sizeof(size_t));
    if (step == 0) step = 1;

    size_t *p = NULL;
    if (posix_memalign((void**)&p, 64, elems*sizeof(size_t))) return NULL;

    for(size_t i=0;i<elems;i++) p[i] = (i + step) % elems;
    for(size_t i=0;i<elems;i++){ 
        size_t j = (i*2654435761u) % elems;
        size_t t = p[i]; p[i] = p[j]; p[j] = t;
    }
    return p;
}

static size_t* make_stride_ring(size_t bytes, size_t stride, size_t* out_count){
    if (stride == 0) stride = 64;
    size_t slots = bytes / stride;            
    if (slots < 2) slots = 2;
    size_t *r = NULL;
    if (posix_memalign((void**)&r, 64, slots*sizeof(size_t))) return NULL;
    for (size_t i=0;i<slots;i++) r[i] = (i + 1) % slots; 
    for (size_t i=0;i<slots;i++){ 
        size_t j = (i*11400714819323198485ull) % slots;  
        size_t t = r[i]; r[i] = r[j]; r[j] = t;
    }
    if (out_count) *out_count = slots;
    return r;
}

static size_t parse_size(const char* s){
    char *end; uint64_t v = strtoull(s, &end, 10);
    if (*end == 0) return (size_t)v;
    if (*end=='K' || *end=='k') return (size_t)(v<<10);
    if (*end=='M' || *end=='m') return (size_t)(v<<20);
    if (*end=='G' || *end=='g') return (size_t)(v<<30);
    return (size_t)v;
}

int main(int argc, char** argv){
    size_t bytes = 32<<20;     
    size_t stride = 64;        
    size_t iters  = 64;        
    enum {P_RAND_READ, P_SEQ_READ, P_SEQ_WRITE, P_RAND_WRITE} pat = P_RAND_READ;

    for(int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--size") && i+1<argc)    bytes  = parse_size(argv[++i]);
        else if (!strcmp(argv[i],"--stride") && i+1<argc) stride = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i],"--iters") && i+1<argc)  iters  = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i],"--pattern") && i+1<argc){
            char* p = argv[++i];
            if      (!strcmp(p,"rand_read"))  pat = P_RAND_READ;
            else if (!strcmp(p,"seq_read"))   pat = P_SEQ_READ;
            else if (!strcmp(p,"seq_write"))  pat = P_SEQ_WRITE;
            else if (!strcmp(p,"rand_write")) pat = P_RAND_WRITE;
            else { fprintf(stderr,"Unknown --pattern\n"); return 1; }
        }
    }

    pin_core0();

    uint8_t *bufA=NULL, *bufB=NULL;
    if (posix_memalign((void**)&bufA, 64, bytes)) { perror("alloc A"); return 1; }
    if (posix_memalign((void**)&bufB, 64, bytes)) { perror("alloc B"); return 1; }
    memset(bufA, 0xA5, bytes);
    memset(bufB, 0x5A, bytes);

    size_t *ring_rand_read = NULL;
    size_t *ring_rand_write = NULL; size_t slots_rw = 0;

    if (pat==P_RAND_READ){
        ring_rand_read = make_ring(bytes, stride);
        if (!ring_rand_read){ fprintf(stderr,"alloc ring (rand_read) failed\n"); return 1; }
    } else if (pat==P_RAND_WRITE){
        ring_rand_write = make_stride_ring(bytes, stride, &slots_rw);
        if (!ring_rand_write){ fprintf(stderr,"alloc ring (rand_write) failed\n"); return 1; }
    }

    volatile size_t idx = 0;
    size_t warm = 100000;
    if (pat==P_RAND_READ){
        for (size_t i=0;i<warm;i++) idx = ring_rand_read[idx];
    } else if (pat==P_RAND_WRITE){
        for (size_t i=0;i<warm;i++){
            size_t off = (idx * stride);
            if (off >= bytes) off %= bytes;    
            bufA[off] = (uint8_t)i;
            idx = ring_rand_write[idx];
        }
    } else {
        for (size_t i=0;i<warm;i++) { volatile uint8_t x = bufA[(i*stride)%bytes]; (void)x; }
    }

    double t0 = now_ns();
    size_t ops = 0;

    if (pat==P_RAND_READ) {
        size_t deref = (size_t) (50ull*1000*1000); 
        if (iters) deref = iters * 1000000ull;    
        for (size_t i=0;i<deref;i++){ idx = ring_rand_read[idx]; ops++; }

    } else if (pat==P_SEQ_READ) {
        for (size_t k=0;k<iters;k++){
            for (size_t off=0; off<bytes; off+=stride){
                volatile uint8_t x = bufA[off]; (void)x; ops++;
            }
        }

    } else if (pat==P_SEQ_WRITE) {
        for (size_t k=0;k<iters;k++){
            for (size_t off=0; off<bytes; off+=stride){
                bufA[off] = (uint8_t)off; ops++;
            }
        }

    } else {
      
        size_t steps = (size_t) (50ull*1000*1000);  
        if (iters) steps = iters * 1000000ull;
        for (size_t i=0;i<steps;i++){
            size_t off = idx * stride; 
            if (off >= bytes) off %= bytes;
            bufA[off] = (uint8_t)i;     
            idx = ring_rand_write[idx];
            ops++;
        }
    }

    double t1 = now_ns();
    double t_ns = t1 - t0;
    double avg_ns = t_ns / (double)ops;

    const double bytes_per_op = 64.0;
    double total_bytes = bytes_per_op * (double)ops;
    double bw_MBps = (total_bytes / (t_ns/1e9)) / 1e6;

    const char* pname =
        (pat==P_RAND_READ ? "rand_read" :
         pat==P_SEQ_READ  ? "seq_read"  :
         pat==P_SEQ_WRITE ? "seq_write" : "rand_write");

    printf("pattern=%s,stride=%zu,size=%zu,iters=%zu,ops=%zu,avg_ns=%.3f,bw_MBps=%.2f\n",
           pname, stride, bytes, iters, ops, avg_ns, bw_MBps);

    fprintf(stderr,"sink=%zu\n", (size_t)idx);

    free(bufA); free(bufB);
    if (ring_rand_read)  free(ring_rand_read);
    if (ring_rand_write) free(ring_rand_write);
    return 0;
}
