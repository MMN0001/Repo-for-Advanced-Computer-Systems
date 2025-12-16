#include "xor.h"
#include <stdlib.h>
#include <string.h>

static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static inline uint64_t hash64(uint64_t k, uint64_t seed) {
    uint64_t x = k ^ seed;
    return splitmix64(&x);
}

static inline uint32_t rotl32(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

static inline void get3(uint64_t h, uint32_t m, uint32_t *h0, uint32_t *h1, uint32_t *h2) {

    uint32_t a = (uint32_t)(h);
    uint32_t b = (uint32_t)(h >> 32);
    uint32_t c = rotl32(a ^ b, 11) + 0x9e3779b9u;

    *h0 = (a) % m;
    *h1 = (b) % m;
    *h2 = (c) % m;

    if (*h1 == *h0) *h1 = (*h1 + 1) % m;
    if (*h2 == *h0) *h2 = (*h2 + 2) % m;
    if (*h2 == *h1) *h2 = (*h2 + 3) % m;
}

static inline uint16_t fingerprint(uint64_t h, uint32_t fp_bits) {
    uint16_t fp = (uint16_t)(h ^ (h >> 32));
    fp &= (uint16_t)((1u << fp_bits) - 1u);
    if (fp == 0) fp = 1;
    return fp;
}

typedef struct {
    uint32_t xormask;  
    uint8_t  degree;   
} node_t;

typedef struct {
    uint32_t v;
    uint64_t h;
} stack_item_t;

static int xor_try_build(xor_filter_t *xf, const uint64_t *keys, uint32_t nkeys, uint32_t fp_bits, uint32_t seed) {
  
    uint32_t m = (uint32_t)( (double)nkeys * 1.30 ) + 32;
    if (m < 64) m = 64;

    node_t *g = (node_t*)calloc(m, sizeof(node_t));
    if (!g) return 1;

    uint32_t *e0 = (uint32_t*)malloc(sizeof(uint32_t) * nkeys);
    uint32_t *e1 = (uint32_t*)malloc(sizeof(uint32_t) * nkeys);
    uint32_t *e2 = (uint32_t*)malloc(sizeof(uint32_t) * nkeys);
    uint64_t *hh = (uint64_t*)malloc(sizeof(uint64_t) * nkeys);
    if (!e0 || !e1 || !e2 || !hh) {
        free(g); free(e0); free(e1); free(e2); free(hh);
        return 1;
    }

    for (uint32_t i = 0; i < nkeys; i++) {
        uint64_t h = hash64(keys[i], ((uint64_t)seed << 32) ^ 0xD6E8FEB86659FD93ULL);
        hh[i] = h;
        uint32_t a,b,c;
        get3(h, m, &a, &b, &c);
        e0[i]=a; e1[i]=b; e2[i]=c;

        g[a].degree++; g[a].xormask ^= i;
        g[b].degree++; g[b].xormask ^= i;
        g[c].degree++; g[c].xormask ^= i;
    }

  
    uint32_t *queue = (uint32_t*)malloc(sizeof(uint32_t) * m);
    stack_item_t *stack = (stack_item_t*)malloc(sizeof(stack_item_t) * nkeys);
    if (!queue || !stack) {
        free(g); free(e0); free(e1); free(e2); free(hh); free(queue); free(stack);
        return 1;
    }

    uint32_t qh=0, qt=0;
    for (uint32_t v=0; v<m; v++) if (g[v].degree == 1) queue[qt++] = v;

    uint32_t sp = 0;
    while (qh < qt) {
        uint32_t v = queue[qh++];
        if (g[v].degree != 1) continue;
        uint32_t eid = g[v].xormask;

      
        stack[sp++] = (stack_item_t){ .v = v, .h = hh[eid] };

    
        uint32_t a=e0[eid], b=e1[eid], c=e2[eid];
        uint32_t vs[3] = {a,b,c};
        for (int t=0;t<3;t++){
            uint32_t u = vs[t];
            if (g[u].degree == 0) continue;
            g[u].degree--;
            g[u].xormask ^= eid;
            if (g[u].degree == 1) queue[qt++] = u;
        }
    }

  
    if (sp != nkeys) {
        free(g); free(e0); free(e1); free(e2); free(hh); free(queue); free(stack);
        return 1;
    }


    xf->n = m;
    xf->fp_bits = fp_bits;
    xf->seed = seed;
    xf->fps = (uint16_t*)calloc(m, sizeof(uint16_t));
    if (!xf->fps) {
        free(g); free(e0); free(e1); free(e2); free(hh); free(queue); free(stack);
        return 1;
    }


    while (sp > 0) {
        stack_item_t it = stack[--sp];
        uint32_t v = it.v;
        uint64_t h = it.h;

        uint32_t a,b,c;
        get3(h, m, &a, &b, &c);
        uint16_t fp = fingerprint(h, fp_bits);

        uint16_t val = fp ^ xf->fps[a] ^ xf->fps[b] ^ xf->fps[c];
        xf->fps[v] = val;
    }

    free(g); free(e0); free(e1); free(e2); free(hh); free(queue); free(stack);
    return 0;
}

int xor_build(xor_filter_t *xf, const uint64_t *keys, uint32_t nkeys, uint32_t fp_bits, uint32_t seed) {
    if (!xf || !keys || nkeys == 0) return 1;
    if (!(fp_bits == 8 || fp_bits == 12 || fp_bits == 16)) return 1;

    memset(xf, 0, sizeof(*xf));


    for (uint32_t t = 0; t < 50; t++) {
        uint32_t s = seed + 0x9e3779b9u * t;
        xor_filter_t tmp = {0};
        if (xor_try_build(&tmp, keys, nkeys, fp_bits, s) == 0) {
            *xf = tmp;
            return 0;
        }
        xor_free(&tmp);
    }
    return 1;
}

int xor_query(const xor_filter_t *xf, uint64_t key) {
    if (!xf || !xf->fps || xf->n == 0) return 0;
    uint64_t h = hash64(key, ((uint64_t)xf->seed << 32) ^ 0xD6E8FEB86659FD93ULL);
    uint32_t a,b,c;
    get3(h, xf->n, &a, &b, &c);
    uint16_t fp = fingerprint(h, xf->fp_bits);
    uint16_t got = xf->fps[a] ^ xf->fps[b] ^ xf->fps[c];
    return (got == fp);
}

void xor_free(xor_filter_t *xf) {
    if (!xf) return;
    free(xf->fps);
    memset(xf, 0, sizeof(*xf));
}

size_t xor_bytes(const xor_filter_t *xf) {
    if (!xf || !xf->fps) return 0;
    return (size_t)xf->n * sizeof(uint16_t);
}
