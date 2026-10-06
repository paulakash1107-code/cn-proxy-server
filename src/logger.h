#ifndef LOGGER_H
#define LOGGER_H
#include <stddef.h>

/* cache_status: "HIT", "MISS", "BYPASS" or "BLOCKED". Must be thread-safe. */
void log_request(const char *client_ip, const char *method, const char *url,
                 int status, size_t bytes, const char *cache_status, double ms);
#endif
