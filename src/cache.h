#ifndef CACHE_H
#define CACHE_H
#include <stddef.h>
#include <time.h>

/* Returns 1 = fresh hit, 2 = stale (needs revalidation), 0 = miss.
   On 1 or 2: *resp is malloc'd (caller frees), etag_out is a 128-byte buffer. */
int  cache_get(const char *key, char **resp, size_t *len, char *etag_out);
void cache_put(const char *key, const char *resp, size_t len, time_t ttl, const char *etag);
void cache_refresh(const char *key, time_t ttl);   /* after a 304 Not Modified */
#endif
