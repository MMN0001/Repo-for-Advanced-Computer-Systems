#ifndef CUCKOO_FILTER_H
#define CUCKOO_FILTER_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t fp;
} ck_slot_t;

typedef struct {
    size_t insert_calls;
    size_t insert_fails;
    size_t total_kicks;
    size_t total_probes;
    size_t stash_inserts;
    size_t stash_hits;
} cuckoo_stats_t;

typedef struct {
    size_t nbuckets;
    size_t bucket_size;
    size_t nitems;

    int fp_bits;
    uint32_t fp_mask;

    ck_slot_t *table;

    cuckoo_stats_t stats;

    size_t stash_cap;
    size_t stash_size;
    uint64_t *stash_keys;
    uint32_t *stash_fp;
} cuckoo_filter_t;

int cuckoo_init(cuckoo_filter_t *cf, size_t nkeys_hint, int fp_bits);

void cuckoo_free(cuckoo_filter_t *cf);

int cuckoo_insert(cuckoo_filter_t *cf, uint64_t key);

int cuckoo_query(cuckoo_filter_t *cf, uint64_t key);

int cuckoo_delete(cuckoo_filter_t *cf, uint64_t key);

size_t cuckoo_bytes(const cuckoo_filter_t *cf);

static inline size_t cuckoo_capacity_slots(const cuckoo_filter_t *cf) {
    return cf->nbuckets * cf->bucket_size;
}

#endif
