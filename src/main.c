#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "proxy.h"
#include "http.h"
#include "access.h"
#include "stats.h"

#define MAX_CLIENTS 200   /* simultaneous connections; beyond this we answer 503 */

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
} client_t;

static pthread_mutex_t active_lock = PTHREAD_MUTEX_INITIALIZER;
static int active_clients = 0;
static volatile sig_atomic_t stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    stop = 1;                       /* accept() is interrupted, loop ends, program exits cleanly */
}

static void *thread_main(void *arg) {
    client_t *c = arg;
    handle_client(c->fd, c->ip);
    close(c->fd);
    free(c);
    pthread_mutex_lock(&active_lock);
    active_clients--;
    pthread_mutex_unlock(&active_lock);
    return NULL;
}

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8888;
    signal(SIGPIPE, SIG_IGN);       /* don't die when a client disconnects mid-send */

    (void)access_init();
    /* usage: ./proxy PORT [--block host] [--block host] ... */
    for (int i = 2; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--block") == 0) access_block_host(argv[i + 1]);
    }

    struct sigaction sa;            /* no SA_RESTART, so accept() returns when Ctrl+C is pressed */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(srv, 128) < 0) { perror("listen"); return 1; }
    printf("Proxy listening on port %d\n", port);

    while (!stop) {
        struct sockaddr_in ca;
        socklen_t len = sizeof ca;
        int fd = accept(srv, (struct sockaddr *)&ca, &len);
        if (fd < 0) continue;       /* EINTR on Ctrl+C: loop condition ends the server */

        struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };   /* no thread hangs forever */
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

        pthread_mutex_lock(&active_lock);
        int full = active_clients >= MAX_CLIENTS;
        if (!full) active_clients++;
        pthread_mutex_unlock(&active_lock);
        if (full) {                 /* overloaded: refuse politely instead of crashing */
            send_error(fd, 503, "Service Unavailable");
            close(fd);
            continue;
        }

        client_t *c = malloc(sizeof *c);
        if (!c) {
            pthread_mutex_lock(&active_lock); active_clients--; pthread_mutex_unlock(&active_lock);
            close(fd);
            continue;
        }
        c->fd = fd;
        inet_ntop(AF_INET, &ca.sin_addr, c->ip, sizeof c->ip);

        pthread_t t;
        if (pthread_create(&t, NULL, thread_main, c) == 0) {
            pthread_detach(t);
        } else {
            pthread_mutex_lock(&active_lock); active_clients--; pthread_mutex_unlock(&active_lock);
            close(fd);
            free(c);
        }
    }

    printf("\nShutting down...\n");
    stats_print();
    close(srv);
    access_destroy();
    return 0;
}
