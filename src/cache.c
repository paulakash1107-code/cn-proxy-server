#include "cache.h"

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
