#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include "tunnel.h"
#include "proxy.h"
#include "http.h"
#include "logger.h"
#include "access.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

void handle_connect(int client_fd, const char *client_ip, const char *target) {
    double t0 = now_ms();
    char host[256];
    int port = 443;

    /* target looks like "example.com:443" */
    if (strlen(target) >= sizeof host) {
        send_error(client_fd, 400, "Bad Request");
        return;
    }
    strcpy(host, target);
    char *colon = strrchr(host, ':');
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }
    if (host[0] == '\0' || port <= 0 || port > 65535) {
        send_error(client_fd, 400, "Bad Request");
        log_request(client_ip, "CONNECT", target, 400, 0, "-", now_ms() - t0);
        return;
    }

    int astatus = 403;
    if (!access_is_allowed(host)) {
        send_error(client_fd, astatus, astatus == 429 ? "Too Many Requests" : "Forbidden");
        log_request(client_ip, "CONNECT", target, astatus, 0, "BLOCKED", now_ms() - t0);
        return;
    }

    int ofd = connect_to_host(host, port);
    if (ofd < 0) {
        send_error(client_fd, 502, "Bad Gateway");
        log_request(client_ip, "CONNECT", target, 502, 0, "-", now_ms() - t0);
        return;
    }

    const char *ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
    if (send_all(client_fd, ok, strlen(ok)) < 0) {
        close(ofd);
        return;
    }

    /* Relay bytes in both directions until either side closes or goes idle. */
    char buf[8192];
    size_t total = 0;
    int maxfd = (client_fd > ofd ? client_fd : ofd) + 1;

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(client_fd, &rfds);
        FD_SET(ofd, &rfds);
        struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };   /* idle timeout */

        int r = select(maxfd, &rfds, NULL, NULL, &tv);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;

        if (FD_ISSET(client_fd, &rfds)) {
            ssize_t n = recv(client_fd, buf, sizeof buf, 0);
            if (n <= 0) break;
            if (send_all(ofd, buf, (size_t)n) < 0) break;
            total += (size_t)n;
        }
        if (FD_ISSET(ofd, &rfds)) {
            ssize_t n = recv(ofd, buf, sizeof buf, 0);
            if (n <= 0) break;
            if (send_all(client_fd, buf, (size_t)n) < 0) break;
            total += (size_t)n;
        }
    }

    close(ofd);
    log_request(client_ip, "CONNECT", target, 200, total, "TUNNEL", now_ms() - t0);
}
