#include <stdio.h>
#include "logger.h"

/* STUB: owner = Anik. Replace with thread-safe file logger. */
void log_request(const char *client_ip, const char *method, const char *url,
                 int status, size_t bytes, const char *cache_status, double ms) {
    printf("%s %s %s %d %zu %s %.1fms\n", client_ip, method, url, status, bytes, cache_status, ms);
}
