#ifndef STATS_H
#define STATS_H
#include <stddef.h>

/* Called once per finished request (thread-safe). cache_status: "HIT", "MISS", "TUNNEL", "BLOCKED", "-". */
void stats_record(int status, size_t bytes, const char *cache_status);

/* Prints a summary (total requests, hits, misses, blocked, bytes). Called at shutdown. */
void stats_print(void);
#endif
