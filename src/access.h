#ifndef ACCESS_H
#define ACCESS_H

/*
 * Initialize access control.
 */
int access_init(void);

/*
 * Check whether a host is allowed.
 *
 * Returns:
 *   1 -> allowed
 *   0 -> blocked
 */
int access_is_allowed(const char *host);

/*
 * Add a host to the block list.
 */
int access_block_host(const char *host);

/*
 * Remove a host from the block list.
 */
int access_unblock_host(const char *host);

/*
 * Free access-control resources.
 */
void access_destroy(void);


/*
 * Per-client rate limiting (token bucket).
 * Returns 1 = allowed, 0 = over the limit (the proxy answers 429).
 * Disabled (always 1) until access_set_rate() is called with per_sec > 0.
 */
int access_rate_ok(const char *client_ip);

/* per_sec = tokens added per second, burst = bucket size. per_sec <= 0 disables the limiter. */
void access_set_rate(double per_sec, int burst);

/*
 * Loads a config file. Lines: "block <host>", "rate <per_sec> <burst>", "# comment".
 * Returns the number of settings applied, or -1 if the file cannot be opened.
 */
int access_load_config(const char *path);

#endif
