#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static inline double now_sec(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char** argv){
  size_t bytes = (argc >= 2) ? (size_t)atoll(argv[1]) : (size_t)512 * 1024 * 1024; 
  int iters = (argc >= 3) ? atoi(argv[2]) : 20;

  size_t n = bytes / sizeof(float);
  float *a, *b, *c;
  if (posix_memalign((void**)&a, 64, n*sizeof(float))) return 1;
  if (posix_memalign((void**)&b, 64, n*sizeof(float))) return 1;
  if (posix_memalign((void**)&c, 64, n*sizeof(float))) return 1;

  for (size_t i=0;i<n;i++){ a[i]=1.0f; b[i]=2.0f; c[i]=3.0f; }
  const float s = 3.0f;

  for (int t=0;t<3;t++){
    for (size_t i=0;i<n;i++) a[i] = b[i] + s*c[i];
  }

  double t0 = now_sec();
  for (int t=0;t<iters;t++){
    for (size_t i=0;i<n;i++) a[i] = b[i] + s*c[i];
  }
  double t1 = now_sec();
  double sec = (t1 - t0) / (double)iters;

  double bytes_moved = 12.0 * (double)n;
  double bw_gbs = (bytes_moved / sec) / 1e9;

  printf("STREAM_TRIAD bytes_per_array=%zu iters=%d\n", bytes, iters);
  printf("avg_time=%.6f s\n", sec);
  printf("bandwidth=%.3f GB/s (triad, ~12B/elem)\n", bw_gbs);

  free(a); free(b); free(c);
  return 0;
}
