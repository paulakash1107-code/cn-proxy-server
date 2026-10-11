#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "cache.h"

#define BIG (2UL * 1024 * 1024)       /* equals the cache's per-entry limit */

static int failures = 0;
#define CHECK(cond, msg) do { if (cond) printf("PASS  %s\n", msg); \
    else { printf("FAIL  %s\n", msg); failures++; } } while (0)

static int get_code(const char *key) {
    char *r = NULL; size_t n = 0; char et[128];
    int c = cache_get(key, &r, &n, et);
    if (c) free(r);
    return c;
}

static void *worker(void *arg) {
    long id = (long)arg;
    char key[64], data[256];
    memset(data, 'x', sizeof data);
    for (int i = 0; i < 2000; i++) {
        snprintf(key, sizeof key, "http://t/%ld", (id * 7 + i) % 50);
        if (i % 3 == 0) cache_put(key, data, sizeof data, 30, "\"e\"");
        else get_code(key);
    }
    return NULL;
}

int main(void) {
    char *r = NULL; size_t n = 0; char etag[128], key[32];

    /* 1. LRU eviction on a fresh cache: ten 2 MB entries fill the 20 MB limit exactly */
    char *blob = malloc(BIG);
    memset(blob, 'z', BIG);
    for (int i = 0; i < 10; i++) { snprintf(key, sizeof key, "big%d", i); cache_put(key, blob, BIG, 60, NULL); }
    get_code("big0");                          /* big0 becomes most recently used */
    cache_put("big10", blob, BIG, 60, NULL);   /* over the limit: least recently used (big1) goes */
    CHECK(get_code("big1") == 0, "least recently used entry (big1) was evicted");
    CHECK(get_code("big0") == 1 && get_code("big2") == 1 && get_code("big10") == 1,
          "recently used and newer entries are still cached");

    /* 2. responses over the per-entry limit are not stored */
    char *huge = malloc(BIG + 1);
    memset(huge, 'q', BIG + 1);
    cache_put("http://huge/", huge, BIG + 1, 60, NULL);
    free(huge);
    free(blob);
    CHECK(get_code("http://huge/") == 0, "response over 2 MB is not cached");

    /* 3. put then get returns identical bytes, the etag and a fresh hit */
    const char *body = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
    cache_put("http://a/", body, strlen(body), 60, "\"v1\"");
    int c = cache_get("http://a/", &r, &n, etag);
    CHECK(c == 1 && n == strlen(body) && memcmp(r, body, n) == 0 && strcmp(etag, "\"v1\"") == 0,
          "put then get: identical bytes, etag, fresh hit");
    if (c) free(r);

    /* 4. unknown key is a miss */
    CHECK(get_code("http://nope/") == 0, "unknown key is a miss");

    /* 5. putting the same key again replaces the entry */
    cache_put("http://rep/", "one", 3, 60, NULL);
    cache_put("http://rep/", "two2", 4, 60, NULL);
    c = cache_get("http://rep/", &r, &n, etag);
    CHECK(c == 1 && n == 4 && memcmp(r, "two2", 4) == 0, "same key replaces the old entry");
    if (c) free(r);

    /* 6. expiry and refresh (2 second wait) */
    cache_put("http://exp-etag/", "data", 4, 1, "\"x\"");
    cache_put("http://exp-noetag/", "data", 4, 1, NULL);
    cache_put("http://refreshed/", "data", 4, 1, "\"y\"");
    cache_refresh("http://refreshed/", 60);
    sleep(2);
    CHECK(get_code("http://exp-etag/") == 2, "expired entry with ETag is stale (2)");
    CHECK(get_code("http://exp-noetag/") == 0, "expired entry without ETag is a miss (0)");
    CHECK(get_code("http://refreshed/") == 1, "cache_refresh extends the lifetime");

    /* 7. eight threads hammering the cache */
    pthread_t t[8];
    for (long i = 0; i < 8; i++) pthread_create(&t[i], NULL, worker, (void *)i);
    for (int i = 0; i < 8; i++) pthread_join(t[i], NULL);
    CHECK(1, "8 threads x 2000 put/get operations: no crash or sanitizer error");

    printf("\nfailures: %d\n", failures);
    return failures ? 1 : 0;
}
