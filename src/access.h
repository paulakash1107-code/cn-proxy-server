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

#endif
