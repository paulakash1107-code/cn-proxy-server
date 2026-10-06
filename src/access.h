#ifndef ACCESS_H
#define ACCESS_H

/* Returns 1 = allowed, 0 = blocked. When blocked, *status is set to 403 or 429. */
int access_check(const char *client_ip, const char *host, int *status);
#endif
