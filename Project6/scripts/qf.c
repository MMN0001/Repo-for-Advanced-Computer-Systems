#include "qf.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static inline uint64_t hash64(uint64_t k) {
    uint64_t x = k;
    return splitmix64(&x);
}

static inline size_t inc(size_t i, size_t n) { return (i + 1) & (n - 1); }
static inline size_t dec(size_t i, size_t n) { return (i - 1) & (n - 1); }

static inline int slot_empty(const qf_slot_t *s) {
    return (s->rem == 0) && (s->continuation == 0) && (s->shifted == 0);
}

static inline void clear_elem_keep_occupied(qf_slot_t *s) {
    s->rem = 0;
    s->continuation = 0;
    s->shifted = 0;
}

static inline void copy_elem_keep_dst_occupied(qf_slot_t *dst, const qf_slot_t *src) {
    dst->rem = src->rem;
    dst->continuation = src->continuation;
    dst->shifted = src->shifted;
}

static inline size_t home_index(uint64_t h, size_t qbits) {
    return (size_t)(h & ((1ULL << qbits) - 1));
}
static inline uint16_t rem_bits(uint64_t h, size_t qbits, size_t rbits) {
    uint64_t mask = (rbits == 64) ? ~0ULL : ((1ULL << rbits) - 1ULL);
    uint16_t r = (uint16_t)((h >> qbits) & mask);
    return (r == 0) ? 1 : r;
}

static size_t find_cluster_start(const quotient_filter_t *qf, size_t q) {
    size_t i = q;
    while (qf->slots[i].shifted) {
        i = dec(i, qf->nslots);
    }
    return i;
}

static size_t find_run_start(const quotient_filter_t *qf, size_t q) {
    size_t c = find_cluster_start(qf, q);

    size_t s = c;
    while (slot_empty(&qf->slots[s])) {
        s = inc(s, qf->nslots);
    }

    for (size_t b = c; b != q; b = inc(b, qf->nslots)) {
        if (qf->slots[b].occupied) {
            s = inc(s, qf->nslots);
            while (qf->slots[s].continuation) {
                s = inc(s, qf->nslots);
            }
            while (slot_empty(&qf->slots[s])) {
                s = inc(s, qf->nslots);
            }
        }
    }
    return s;
}

static void shift_right_make_hole(quotient_filter_t *qf, size_t pos) {
    size_t i = pos;
    while (!slot_empty(&qf->slots[i])) {
        i = inc(i, qf->nslots);
    }
    while (i != pos) {
        size_t p = dec(i, qf->nslots);
        copy_elem_keep_dst_occupied(&qf->slots[i], &qf->slots[p]);
        qf->slots[i].shifted = 1;
        clear_elem_keep_occupied(&qf->slots[p]);
        i = p;
    }
}

static void shift_left_fill_hole(quotient_filter_t *qf, size_t pos) {
    size_t cur = pos;
    while (1) {
        size_t nxt = inc(cur, qf->nslots);
        if (slot_empty(&qf->slots[nxt])) {
            clear_elem_keep_occupied(&qf->slots[cur]);
            return;
        }
        copy_elem_keep_dst_occupied(&qf->slots[cur], &qf->slots[nxt]);
        clear_elem_keep_occupied(&qf->slots[nxt]);
        cur = nxt;
    }
}

int qf_init(quotient_filter_t *qf, size_t qbits, size_t rbits) {
    if (!qf || qbits == 0 || rbits == 0 || rbits > 16) {
        return EINVAL;
    }

    memset(qf, 0, sizeof(*qf));
    qf->qbits = qbits;
    qf->rbits = rbits;
    qf->nslots = 1ULL << qbits;

    qf->slots = (qf_slot_t*)calloc(qf->nslots, sizeof(qf_slot_t));
    if (!qf->slots) {
        return ENOMEM;
    }
    return 0;
}

void qf_free(quotient_filter_t *qf) {
    if (!qf) return;
    free(qf->slots);
    memset(qf, 0, sizeof(*qf));
}

int qf_query(const quotient_filter_t *qf, uint64_t key) {
    uint64_t h = hash64(key);
    size_t q = home_index(h, qf->qbits);
    uint16_t r = rem_bits(h, qf->qbits, qf->rbits);

    if (!qf->slots[q].occupied) return 0;

    size_t s = find_run_start(qf, q);
    size_t i = s;

    while (1) {
        if (slot_empty(&qf->slots[i])) return 0;
        if (qf->slots[i].rem == r) return 1;

        size_t nxt = inc(i, qf->nslots);
        if (!qf->slots[nxt].continuation) return 0;
        i = nxt;
    }
}

int qf_insert(quotient_filter_t *qf, uint64_t key) {
    if (qf_load_factor(qf) > 0.95) return 1;

    uint64_t h = hash64(key);
    size_t q = home_index(h, qf->qbits);
    uint16_t r = rem_bits(h, qf->qbits, qf->rbits);

    if (qf_query(qf, key)) return 0;

    if (!qf->slots[q].occupied && slot_empty(&qf->slots[q])) {
        qf->slots[q].occupied = 1;
        qf->slots[q].rem = r;
        qf->slots[q].continuation = 0;
        qf->slots[q].shifted = 0;
        qf->nitems++;
        return 0;
    }

    qf->slots[q].occupied = 1;

    size_t run_start = find_run_start(qf, q);

    size_t ins = run_start;
    while (1) {
        if (slot_empty(&qf->slots[ins])) break; 
        if (qf->slots[ins].rem > r) break;

        size_t nxt = inc(ins, qf->nslots);
        if (!qf->slots[nxt].continuation) 
            ins = nxt;
            break;
        }
        ins = nxt;
    }

    shift_right_make_hole(qf, ins);

    qf->slots[ins].rem = r;
    qf->slots[ins].shifted = (ins != q);

    if (ins == run_start) {
        qf->slots[ins].continuation = 0;

        size_t nxt = inc(ins, qf->nslots);
        if (!slot_empty(&qf->slots[nxt])) {
            qf->slots[nxt].continuation = 1;
            qf->slots[nxt].shifted = 1;
        }
    } else {
        qf->slots[ins].continuation = 1;
    }


    qf->slots[run_start].continuation = 0;

    qf->nitems++;
    return 0;
}


int qf_delete(quotient_filter_t *qf, uint64_t key) {
    uint64_t h = hash64(key);
    size_t q = home_index(h, qf->qbits);
    uint16_t r = rem_bits(h, qf->qbits, qf->rbits);

    if (!qf->slots[q].occupied) return 1;

    size_t run_start = find_run_start(qf, q);
    size_t i = run_start;

    while (1) {
        if (slot_empty(&qf->slots[i])) return 1;
        if (qf->slots[i].rem == r) break;

        size_t nxt = inc(i, qf->nslots);
        if (!qf->slots[nxt].continuation) return 1;
        i = nxt;
    }

    int removed_was_run_start = (i == run_start);

   
    shift_left_fill_hole(qf, i);
    if (qf->nitems) qf->nitems--;

  
    if (removed_was_run_start) {
        if (!slot_empty(&qf->slots[run_start])) {
            qf->slots[run_start].continuation = 0;
        }
    }

    size_t check = find_run_start(qf, q);
    if (slot_empty(&qf->slots[check])) {
        qf->slots[q].occupied = 0;
    }

    return 0;
}

double qf_load_factor(const quotient_filter_t *qf) {
    return (double)qf->nitems / (double)qf->nslots;
}

size_t qf_bytes(const quotient_filter_t *qf) {
    return qf->nslots * sizeof(qf_slot_t);
}
