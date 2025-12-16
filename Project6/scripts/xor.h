#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t n;        // number of fingerprints
    uint32_t fp_bits;  
    uint32_t seed;
    uint16_t *fps;     
} xor_filter_t;

int  xor_build(xor_filter_t *xf, const uint64_t *keys, uint32_t nkeys, uint32_t fp_bits, uint32_t seed);


int  xor_query(const xor_filter_t *xf, uint64_t key);


void xor_free(xor_filter_t *xf);


size_t xor_bytes(const xor_filter_t *xf);
