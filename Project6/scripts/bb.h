#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *blocks;        
    size_t   nblocks;     
    uint32_t k;            
    size_t   n_keys_hint;   
    double   target_fpr;  
} blocked_bloom_t;

int  blocked_bloom_init(blocked_bloom_t *bf, size_t n_keys, double target_fpr);

int  blocked_bloom_insert(blocked_bloom_t *bf, uint64_t key);

int  blocked_bloom_query(const blocked_bloom_t *bf, uint64_t key);

size_t blocked_bloom_bytes(const blocked_bloom_t *bf);

void blocked_bloom_free(blocked_bloom_t *bf);
