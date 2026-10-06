#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "proxy.h"

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
} client_t;

static void *thread_main(void *arg) {
    client_t *c = arg;
    handle_client(c->fd, c->ip);
    close(c->fd);
    free(c);
    return NULL;
}

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8888;
    signal(SIGPIPE, SIG_IGN);   /* don't die when a client disconnects mid-send */

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

    for (;;) {
        struct sockaddr_in ca;
        socklen_t len = sizeof ca;
        int fd = accept(srv, (struct sockaddr *)&ca, &len);
        if (fd < 0) continue;

        struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };   /* no thread hangs forever */
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

        client_t *c = malloc(sizeof *c);
        if (!c) { close(fd); continue; }
        c->fd = fd;
        inet_ntop(AF_INET, &ca.sin_addr, c->ip, sizeof c->ip);

        pthread_t t;
        if (pthread_create(&t, NULL, thread_main, c) == 0) pthread_detach(t);
        else { close(fd); free(c); }
    }
}
