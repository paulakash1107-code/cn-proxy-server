#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "stats.h"

static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long total_requests = 0;
static unsigned long hits = 0;
static unsigned long misses = 0;
static unsigned long blocked = 0;
static unsigned long tunnels = 0;
static unsigned long errors = 0;       /* status >= 400, not counting blocked */
static unsigned long long bytes_served = 0;

void stats_record(int status, size_t bytes, const char *cache_status) {
    pthread_mutex_lock(&stats_lock);
    total_requests++;
    bytes_served += bytes;
    if (strcmp(cache_status, "HIT") == 0)          hits++;
    else if (strcmp(cache_status, "MISS") == 0)    misses++;
    else if (strcmp(cache_status, "BLOCKED") == 0) blocked++;
    else if (strcmp(cache_status, "TUNNEL") == 0)  tunnels++;
    if (status >= 400 && strcmp(cache_status, "BLOCKED") != 0) errors++;
    pthread_mutex_unlock(&stats_lock);
}

void stats_print(void) {
    pthread_mutex_lock(&stats_lock);
    unsigned long lookups = hits + misses;
    double ratio = lookups ? 100.0 * (double)hits / (double)lookups : 0.0;
    printf("\n===== Proxy statistics =====\n");
    printf("Total requests : %lu\n", total_requests);
    printf("Cache hits     : %lu\n", hits);
    printf("Cache misses   : %lu\n", misses);
    printf("Hit ratio      : %.1f%%\n", ratio);
    printf("Blocked        : %lu\n", blocked);
    printf("HTTPS tunnels  : %lu\n", tunnels);
    printf("Errors (>=400) : %lu\n", errors);
    printf("Bytes served   : %llu\n", bytes_served);
    printf("============================\n");
    pthread_mutex_unlock(&stats_lock);
}
