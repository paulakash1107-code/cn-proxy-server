#include "access.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_BLOCKED_HOSTS 256
#define MAX_HOST_LENGTH 256

static char *blocked_hosts[MAX_BLOCKED_HOSTS];
static int blocked_count = 0;

static pthread_mutex_t access_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * Check whether a host matches a blocked entry.
 *
 * Supports:
 *   example.com
 *   www.example.com
 *   *.example.com
 */
static int host_matches(const char *host, const char *blocked)
{
    if (strcasecmp(host, blocked) == 0)
        return 1;

    /*
     * Wildcard support.
     */
    if (blocked[0] == '*' && blocked[1] == '.')
    {
        const char *domain = blocked + 1;

        size_t host_len = strlen(host);
        size_t domain_len = strlen(domain);

        if (host_len >= domain_len)
        {
            const char *pos = host + host_len - domain_len;

            if (strcasecmp(pos, domain) == 0)
                return 1;
        }
    }

    return 0;
}


int access_init(void)
{
    pthread_mutex_lock(&access_mutex);

    blocked_count = 0;

    for (int i = 0; i < MAX_BLOCKED_HOSTS; i++)
        blocked_hosts[i] = NULL;

    pthread_mutex_unlock(&access_mutex);

    return 0;
}


int access_is_allowed(const char *host)
{
    if (host == NULL || strlen(host) == 0)
        return 0;

    pthread_mutex_lock(&access_mutex);

    for (int i = 0; i < blocked_count; i++)
    {
        if (blocked_hosts[i] &&
            host_matches(host, blocked_hosts[i]))
        {
            pthread_mutex_unlock(&access_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&access_mutex);

    return 1;
}


int access_block_host(const char *host)
{
    if (host == NULL || strlen(host) == 0)
        return -1;

    pthread_mutex_lock(&access_mutex);

    if (blocked_count >= MAX_BLOCKED_HOSTS)
    {
        pthread_mutex_unlock(&access_mutex);
        return -1;
    }

    /*
     * Avoid duplicate entries.
     */
    for (int i = 0; i < blocked_count; i++)
    {
        if (strcasecmp(blocked_hosts[i], host) == 0)
        {
            pthread_mutex_unlock(&access_mutex);
            return 0;
        }
    }

    blocked_hosts[blocked_count] = strdup(host);

    if (blocked_hosts[blocked_count] == NULL)
    {
        pthread_mutex_unlock(&access_mutex);
        return -1;
    }

    blocked_count++;

    pthread_mutex_unlock(&access_mutex);

    return 0;
}


int access_unblock_host(const char *host)
{
    if (host == NULL)
        return -1;

    pthread_mutex_lock(&access_mutex);

    for (int i = 0; i < blocked_count; i++)
    {
        if (strcasecmp(blocked_hosts[i], host) == 0)
        {
            free(blocked_hosts[i]);

            for (int j = i; j < blocked_count - 1; j++)
                blocked_hosts[j] = blocked_hosts[j + 1];

            blocked_hosts[blocked_count - 1] = NULL;

            blocked_count--;

            pthread_mutex_unlock(&access_mutex);

            return 0;
        }
    }

    pthread_mutex_unlock(&access_mutex);

    return -1;
}


void access_destroy(void)
{
    pthread_mutex_lock(&access_mutex);

    for (int i = 0; i < blocked_count; i++)
    {
        free(blocked_hosts[i]);
        blocked_hosts[i] = NULL;
    }

    blocked_count = 0;

    pthread_mutex_unlock(&access_mutex);
}
