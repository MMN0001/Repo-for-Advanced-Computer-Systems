#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t  occupied;
    uint8_t  continuation;
    uint8_t  shifted;
    uint16_t  rem;  
} qf_slot_t;

typedef struct {
    qf_slot_t *slots;
    size_t     qbits;
    size_t     rbits;
    size_t     nslots;  
    size_t     nitems;
} quotient_filter_t;

int  qf_init(quotient_filter_t *qf, size_t qbits, size_t rbits);
void qf_free(quotient_filter_t *qf);

int  qf_insert(quotient_filter_t *qf, uint64_t key);   
int  qf_query (const quotient_filter_t *qf, uint64_t key); 
int  qf_delete(quotient_filter_t *qf, uint64_t key);   

double qf_load_factor(const quotient_filter_t *qf);
size_t qf_bytes(const quotient_filter_t *qf);
