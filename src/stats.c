#include "stats.h"

/* STUB: owner = Anik. Replace with mutex-protected counters. Keep the signatures. */
void stats_record(int status, size_t bytes, const char *cache_status) {
    (void)status; (void)bytes; (void)cache_status;
}
void stats_print(void) {
}
