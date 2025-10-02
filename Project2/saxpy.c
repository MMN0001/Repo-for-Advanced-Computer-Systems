#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef enum { P_UNIT=0, P_STRIDE=1 } pattern_t;

static double now_ns(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec*1e9 + (double)ts.tv_nsec;
}

static void usage(const char* prog){
    fprintf(stderr,
      "Usage: %s [--pattern unit|stride] [--N <elements>] "
      "[--stride_elems <k>] [--iters <n>]\n"
      "Defaults: pattern=unit N=16777216 stride_elems=4 iters=5\n", prog);
}

int main(int argc, char** argv){
    pattern_t pat = P_UNIT;
    size_t N = 16u*1024u*1024u;  
    size_t stride_elems = 4;     
    size_t iters = 5;

    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--pattern") && i+1<argc){
            i++; if(!strcmp(argv[i],"unit")) pat=P_UNIT;
            else if(!strcmp(argv[i],"stride")) pat=P_STRIDE;
            else { usage(argv[0]); return 1; }
        } else if(!strcmp(argv[i],"--N") && i+1<argc){
            N = strtoull(argv[++i], NULL, 10);
        } else if(!strcmp(argv[i],"--stride_elems") && i+1<argc){
            stride_elems = strtoull(argv[++i], NULL, 10);
            if(stride_elems==0) stride_elems=1;
        } else if(!strcmp(argv[i],"--iters") && i+1<argc){
            iters = strtoull(argv[++i], NULL, 10);
            if(iters<1) iters=1;
        } else { usage(argv[0]); return 1; }
    }

    float *x=NULL, *y=NULL;
    size_t bytes = sizeof(float)*N;
    if(posix_memalign((void**)&x, 64, bytes) || posix_memalign((void**)&y, 64, bytes)){
        fprintf(stderr,"alloc failed\n"); return 1;
    }
    for(size_t i=0;i<N;i++){
        x[i] = 0.5f + (float)(i%101)*1e-3f;
        y[i] = 1.0f + (float)((i*7)%89)*1e-3f;
    }

    const float a = 1.6180339f;
    volatile float checksum = 0.f;


    {
        if(pat==P_UNIT){
            for(size_t j=0;j<N;j++) y[j] = a*x[j] + y[j];
        } else { 
            for(size_t j=0;j<N;j+=stride_elems) y[j] = a*x[j] + y[j];
        }
        checksum += y[(N/3)|1];
    }

    double t0 = now_ns();
    for(size_t r=0;r<iters;r++){
        if(pat==P_UNIT){
            for(size_t j=0;j<N;j++) y[j] = a*x[j] + y[j];
        } else {
            for(size_t j=0;j<N;j+=stride_elems) y[j] = a*x[j] + y[j];
        }
        checksum += y[(r+7)%N];
    }
    double t1 = now_ns();

    double total_ns = t1 - t0;
    double avg_ns   = total_ns / (double)iters;

    size_t touched_elems = (pat==P_UNIT ? N : (N + stride_elems - 1)/stride_elems);
    double bytes_per_iter = 3.0 * 4.0 * (double)touched_elems; 
    double gib_per_s = (bytes_per_iter * (double)iters) / total_ns * 1e9 / (1024.0*1024.0*1024.0);

    const char* s_pat = (pat==P_UNIT? "unit":"stride");
    printf("kernel=SAXPY,dtype=f32,pattern=%s,N=%zu,stride_elems=%zu,iters=%zu,"
           "avg_ns=%.3f,total_ns=%.3f,GiBps=%.6f,checksum=%.6f\n",
           s_pat, N, stride_elems, iters, avg_ns, total_ns, gib_per_s, checksum);

    free(x); free(y);
    return 0;
}
