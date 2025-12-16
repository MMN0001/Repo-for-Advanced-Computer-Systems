#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

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

int main(void) {
    const uint32_t N = 200000;
    const uint32_t Q = 200000;
    const uint32_t fp_bits = 8;

    uint64_t *keys = (uint64_t*)malloc(sizeof(uint64_t) * N);
    if (!keys) return 1;

    uint64_t seed = 123456789ULL;
    for (uint32_t i = 0; i < N; i++) keys[i] = splitmix64(&seed);

    xor_filter_t xf;
    uint64_t t0 = now_ns();
    int ok = xor_build(&xf, keys, N, fp_bits, 1);
    uint64_t t1 = now_ns();
    if (ok != 0) {
        printf("xor_build failed\n");
        free(keys);
        return 1;
    }

    printf("=== XOR Filter Test ===\n");
    printf("N=%u fp_bits=%u n=%u bytes=%zu\n", N, fp_bits, xf.n, xor_bytes(&xf));
    printf("Build time: %.3f ms\n", (double)(t1 - t0) / 1e6);

    uint32_t fn = 0;
    for (uint32_t i = 0; i < N; i++) if (!xor_query(&xf, keys[i])) fn++;
    printf("Positive: FN=%u/%u\n", fn, N);

    seed = 987654321ULL;
    uint32_t fp = 0;
    for (uint32_t i = 0; i < Q; i++) {
        uint64_t k = splitmix64(&seed);
        if (xor_query(&xf, k)) fp++;
    }
    printf("Negative: FP=%u/%u => FPR=%.6f\n", fp, Q, (double)fp/(double)Q);

    xor_free(&xf);
    free(keys);
    return 0;
}
