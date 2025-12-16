#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

typedef struct node {
    int key;
    int value;
    struct node* next;
} node_t;

typedef struct {
    size_t nbuckets;
    node_t** buckets;
    pthread_mutex_t* bucket_locks;
} hashtable_t;

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline uint32_t xorshift32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static inline size_t hash_int(int k) {
    uint32_t x = (uint32_t)k;
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return (size_t)x;
}

static hashtable_t* ht_create(size_t nbuckets) {
    hashtable_t* ht = (hashtable_t*)calloc(1, sizeof(*ht));
    if (!ht) return NULL;

    ht->nbuckets = nbuckets;

    ht->buckets = (node_t**)calloc(nbuckets, sizeof(node_t*));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }

    ht->bucket_locks = (pthread_mutex_t*)malloc(nbuckets * sizeof(pthread_mutex_t));
    if (!ht->bucket_locks) {
        free(ht->buckets);
        free(ht);
        return NULL;
    }

    for (size_t i = 0; i < nbuckets; i++) {
        pthread_mutex_init(&ht->bucket_locks[i], NULL);
    }

    return ht;
}

static void ht_destroy(hashtable_t* ht) {
    if (!ht) return;

    if (ht->bucket_locks) {
        for (size_t i = 0; i < ht->nbuckets; i++) {
            pthread_mutex_destroy(&ht->bucket_locks[i]);
        }
        free(ht->bucket_locks);
    }

    // Free nodes
    for (size_t i = 0; i < ht->nbuckets; i++) {
        node_t* cur = ht->buckets[i];
        while (cur) {
            node_t* nxt = cur->next;
            free(cur);
            cur = nxt;
        }
    }

    free(ht->buckets);
    free(ht);
}

static int ht_insert(hashtable_t* ht, int key, int value) {
    size_t b = hash_int(key) % ht->nbuckets;

    pthread_mutex_lock(&ht->bucket_locks[b]);

    node_t* cur = ht->buckets[b];
    while (cur) {
        if (cur->key == key) {
            cur->value = value; 
            pthread_mutex_unlock(&ht->bucket_locks[b]);
            return 1;
        }
        cur = cur->next;
    }

    node_t* n = (node_t*)malloc(sizeof(node_t));
    if (!n) {
        pthread_mutex_unlock(&ht->bucket_locks[b]);
        return 0;
    }

    n->key = key;
    n->value = value;
    n->next = ht->buckets[b];
    ht->buckets[b] = n;

    pthread_mutex_unlock(&ht->bucket_locks[b]);
    return 1;
}

static int ht_find(hashtable_t* ht, int key, int* out_value) {
    size_t b = hash_int(key) % ht->nbuckets;

    pthread_mutex_lock(&ht->bucket_locks[b]);

    node_t* cur = ht->buckets[b];
    while (cur) {
        if (cur->key == key) {
            if (out_value) *out_value = cur->value;
            pthread_mutex_unlock(&ht->bucket_locks[b]);
            return 1;
        }
        cur = cur->next;
    }

    pthread_mutex_unlock(&ht->bucket_locks[b]);
    return 0;
}

static int ht_erase(hashtable_t* ht, int key) {
    size_t b = hash_int(key) % ht->nbuckets;

    pthread_mutex_lock(&ht->bucket_locks[b]);

    node_t* cur = ht->buckets[b];
    node_t* prev = NULL;
    while (cur) {
        if (cur->key == key) {
            if (prev) prev->next = cur->next;
            else ht->buckets[b] = cur->next;
            free(cur);
            pthread_mutex_unlock(&ht->bucket_locks[b]);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }

    pthread_mutex_unlock(&ht->bucket_locks[b]);
    return 0;
}

typedef enum {
    WL_LOOKUP_ONLY = 0,
    WL_INSERT_ONLY = 1,
    WL_MIXED_70_30 = 2
} workload_t;

typedef struct {
    int tid;
    int nthreads;
    hashtable_t* ht;
    int* keys;
    int nkeys;
    workload_t wl;
    int duration_ms;
    uint32_t rng;
    uint64_t ops;
} worker_arg_t;

static void* worker_main(void* p) {
    worker_arg_t* a = (worker_arg_t*)p;
    uint64_t start = now_ns();
    uint64_t end = start + (uint64_t)a->duration_ms * 1000000ull;

    int sink = 0;

    while (now_ns() < end) {
        int k = a->keys[xorshift32(&a->rng) % (uint32_t)a->nkeys];

        if (a->wl == WL_LOOKUP_ONLY) {
            ht_find(a->ht, k, &sink);
            a->ops++;
        } else if (a->wl == WL_INSERT_ONLY) {
            int v = (int)xorshift32(&a->rng);
            ht_insert(a->ht, k, v);
            a->ops++;
        } else {
            uint32_t r = xorshift32(&a->rng) % 100;
            if (r < 70) {
                ht_find(a->ht, k, &sink);
            } else {
                int v = (int)xorshift32(&a->rng);
                ht_insert(a->ht, k, v);
            }
            a->ops++;
        }
    }

    if (sink == 123456789) fprintf(stderr, "sink=%d\n", sink);
    return NULL;
}

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s --workload lookup|insert|mixed --nkeys N --threads T --duration_ms D [--prefill 0|1] [--nbuckets B]\n"
        "Example: %s --workload mixed --nkeys 100000 --threads 8 --duration_ms 2000 --prefill 1\n",
        prog, prog);
}

static workload_t parse_workload(const char* s) {
    if (!strcmp(s, "lookup")) return WL_LOOKUP_ONLY;
    if (!strcmp(s, "insert")) return WL_INSERT_ONLY;
    if (!strcmp(s, "mixed"))  return WL_MIXED_70_30;
    return (workload_t)-1;
}

int main(int argc, char** argv) {
    workload_t wl = (workload_t)-1;
    int nkeys = 0;
    int threads = 0;
    int duration_ms = 2000;
    int prefill = 1;
    size_t nbuckets = 1 << 20;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--workload") && i + 1 < argc) {
            wl = parse_workload(argv[++i]);
        } else if (!strcmp(argv[i], "--nkeys") && i + 1 < argc) {
            nkeys = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--threads") && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--duration_ms") && i + 1 < argc) {
            duration_ms = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--prefill") && i + 1 < argc) {
            prefill = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--nbuckets") && i + 1 < argc) {
            nbuckets = (size_t)strtoull(argv[++i], NULL, 10);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (wl == (workload_t)-1 || nkeys <= 0 || threads <= 0) {
        usage(argv[0]);
        return 1;
    }

    int* keys = (int*)malloc(sizeof(int) * (size_t)nkeys);
    if (!keys) {
        fprintf(stderr, "malloc keys failed\n");
        return 1;
    }
    for (int i = 0; i < nkeys; i++) keys[i] = i;

    uint32_t seed = 12345u;
    for (int i = nkeys - 1; i > 0; i--) {
        uint32_t j = xorshift32(&seed) % (uint32_t)(i + 1);
        int tmp = keys[i]; keys[i] = keys[j]; keys[j] = tmp;
    }

    hashtable_t* ht = ht_create(nbuckets);
    if (!ht) {
        fprintf(stderr, "ht_create failed\n");
        free(keys);
        return 1;
    }

    if (prefill) {
        for (int i = 0; i < nkeys; i++) {
            ht_insert(ht, keys[i], keys[i] ^ 0x9e3779b9);
        }
    }

    pthread_t* th = (pthread_t*)malloc(sizeof(pthread_t) * (size_t)threads);
    worker_arg_t* args = (worker_arg_t*)calloc((size_t)threads, sizeof(worker_arg_t));
    if (!th || !args) {
        fprintf(stderr, "alloc thread args failed\n");
        ht_destroy(ht);
        free(keys);
        free(th);
        free(args);
        return 1;
    }

    for (int t = 0; t < threads; t++) {
        args[t].tid = t;
        args[t].nthreads = threads;
        args[t].ht = ht;
        args[t].keys = keys;
        args[t].nkeys = nkeys;
        args[t].wl = wl;
        args[t].duration_ms = duration_ms;
        args[t].rng = 0xC001D00Du ^ (uint32_t)(t * 2654435761u);
        args[t].ops = 0;

        int rc = pthread_create(&th[t], NULL, worker_main, &args[t]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
            return 1;
        }
    }

    uint64_t total_ops = 0;
    for (int t = 0; t < threads; t++) {
        pthread_join(th[t], NULL);
        total_ops += args[t].ops;
    }

    double seconds = (double)duration_ms / 1000.0;
    double ops_per_sec = (double)total_ops / seconds;

    const char* wl_name = (wl == WL_LOOKUP_ONLY) ? "lookup"
                        : (wl == WL_INSERT_ONLY) ? "insert"
                        : "mixed";

    printf("%s,%d,%d,%d,%d,%zu,%llu,%.3f\n",
           wl_name, nkeys, threads, duration_ms, prefill, nbuckets,
           (unsigned long long)total_ops, ops_per_sec);

    ht_destroy(ht);
    free(keys);
    free(th);
    free(args);
    return 0;
}
