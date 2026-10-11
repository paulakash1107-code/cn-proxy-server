#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cache.h"

#define HASH_BUCKETS     1024
#define MAX_CACHE_BYTES  (20UL * 1024 * 1024)   /* total size limit: 20 MB */
#define MAX_ENTRY_BYTES  (2UL * 1024 * 1024)    /* per-entry limit: 2 MB   */
#define ETAG_MAX         128

typedef struct cache_entry {
    char   *key;                  /* malloc'd copy of the URL            */
    char   *data;                 /* malloc'd raw HTTP response          */
    size_t  len;                  /* number of bytes in data             */
    char    etag[ETAG_MAX];       /* "" if the response had no ETag      */
    time_t  expires;              /* absolute time when it goes stale    */

    struct cache_entry *hnext;    /* next entry in the same hash bucket  */
    struct cache_entry *prev;     /* LRU list: towards most recent       */
    struct cache_entry *next;     /* LRU list: towards least recent      */
} cache_entry;

static cache_entry *buckets[HASH_BUCKETS];   /* the hash table                     */
static cache_entry *lru_head = NULL;         /* most recently used                 */
static cache_entry *lru_tail = NULL;         /* least recently used (evict first)  */
static size_t       total_bytes = 0;         /* sum of len of all entries          */
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- helpers (all callers must hold cache_lock) ---- */

static unsigned long hash_key(const char *s) {              /* djb2 string hash */
    unsigned long h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

static cache_entry *find_entry(const char *key) {
    for (cache_entry *e = buckets[hash_key(key) % HASH_BUCKETS]; e; e = e->hnext)
        if (strcmp(e->key, key) == 0) return e;
    return NULL;
}

static void lru_unlink(cache_entry *e) {
    if (e->prev) e->prev->next = e->next; else lru_head = e->next;
    if (e->next) e->next->prev = e->prev; else lru_tail = e->prev;
    e->prev = e->next = NULL;
}

static void lru_push_front(cache_entry *e) {                /* e becomes most recently used */
    e->prev = NULL;
    e->next = lru_head;
    if (lru_head) lru_head->prev = e;
    lru_head = e;
    if (!lru_tail) lru_tail = e;
}

/* Remove from hash table and LRU list, then free. */
static void remove_entry(cache_entry *e) {
    cache_entry **pp = &buckets[hash_key(e->key) % HASH_BUCKETS];
    while (*pp && *pp != e) pp = &(*pp)->hnext;
    if (*pp) *pp = e->hnext;
    lru_unlink(e);
    total_bytes -= e->len;
    free(e->key);
    free(e->data);
    free(e);
}

/* Evict least recently used entries until `needed` more bytes fit. */
static void make_room(size_t needed) {
    while (lru_tail && total_bytes + needed > MAX_CACHE_BYTES)
        remove_entry(lru_tail);
}

/* Returns 1 = fresh hit, 2 = stale but revalidatable (has an ETag), 0 = miss. */
int cache_get(const char *key, char **resp, size_t *len, char *etag_out) {
    int result = 0;
    pthread_mutex_lock(&cache_lock);
    cache_entry *e = find_entry(key);
    if (e) {
        int fresh = time(NULL) < e->expires;
        if (!fresh && e->etag[0] == '\0') {
            remove_entry(e);                        /* expired, nothing to revalidate with: drop it */
        } else {
            char *copy = malloc(e->len);            /* hand out a copy: the entry may be evicted later */
            if (copy) {
                memcpy(copy, e->data, e->len);
                *resp = copy;
                *len = e->len;
                if (etag_out) {
                    strncpy(etag_out, e->etag, ETAG_MAX - 1);
                    etag_out[ETAG_MAX - 1] = '\0';
                }
                lru_unlink(e);                      /* now the most recently used */
                lru_push_front(e);
                result = fresh ? 1 : 2;
            }
        }
    }
    pthread_mutex_unlock(&cache_lock);
    return result;
}

void cache_put(const char *key, const char *resp, size_t len, time_t ttl, const char *etag) {
    if (!key || !resp || len == 0 || len > MAX_ENTRY_BYTES || ttl <= 0) return;

    cache_entry *e = calloc(1, sizeof *e);          /* calloc: etag starts as "" */
    char *k = strdup(key);
    char *d = malloc(len);
    if (!e || !k || !d) { free(e); free(k); free(d); return; }
    memcpy(d, resp, len);
    e->key = k;
    e->data = d;
    e->len = len;
    e->expires = time(NULL) + ttl;
    if (etag) strncpy(e->etag, etag, ETAG_MAX - 1);

    pthread_mutex_lock(&cache_lock);
    cache_entry *old = find_entry(key);
    if (old) remove_entry(old);                     /* same URL again: replace the old entry */
    make_room(len);
    unsigned long b = hash_key(key) % HASH_BUCKETS;
    e->hnext = buckets[b];
    buckets[b] = e;
    lru_push_front(e);
    total_bytes += len;
    pthread_mutex_unlock(&cache_lock);
}

void cache_refresh(const char *key, time_t ttl) {   /* origin answered 304 Not Modified */
    pthread_mutex_lock(&cache_lock);
    cache_entry *e = find_entry(key);
    if (e) {
        e->expires = time(NULL) + ttl;
        lru_unlink(e);
        lru_push_front(e);
    }
    pthread_mutex_unlock(&cache_lock);
}
