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

/* STUB: owner = Sania. Replace with LRU cache. */
int cache_get(const char *key, char **resp, size_t *len, char *etag_out) {
    (void)key; (void)resp; (void)len; (void)etag_out;
    return 0;
}
void cache_put(const char *key, const char *resp, size_t len, time_t ttl, const char *etag) {
    (void)key; (void)resp; (void)len; (void)ttl; (void)etag;
}
void cache_refresh(const char *key, time_t ttl) {
    (void)key; (void)ttl;
}
