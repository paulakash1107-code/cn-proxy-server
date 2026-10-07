#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include "proxy.h"
#include "http.h"
#include "logger.h"
#include "tunnel.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Open a TCP connection to the origin server. Returns fd or -1. */
int connect_to_host(const char *host, int port) {
    char portstr[16];
    snprintf(portstr, sizeof portstr, "%d", port);

    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1;

    int fd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);  /* also limits connect() */
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* Headers that must not be forwarded as-is (hop-by-hop, or ones we set ourselves). */
static int skip_header(const char *line) {
    static const char *skip[] = {
        "Host:", "Proxy-Connection:", "Connection:", "Keep-Alive:",
        "Proxy-Authorization:", "TE:", "Upgrade:", "Accept-Encoding:", "Expect:", NULL
    };
    for (int i = 0; skip[i]; i++)
        if (strncasecmp(line, skip[i], strlen(skip[i])) == 0) return 1;
    return 0;
}

/* Rewrite the client's request for the origin: relative path, clean headers. */
static int build_origin_request(char *out, size_t cap, const char *method, const char *path,
                                const char *host, int port, const char *client_ip,
                                const char *req) {
    int off = snprintf(out, cap, "%s %s HTTP/1.1\r\n", method, path);
    const char *p = strstr(req, "\r\n");          /* end of the request line */
    if (!p) return -1;
    p += 2;

    while (*p && !(p[0] == '\r' && p[1] == '\n')) {
        const char *eol = strstr(p, "\r\n");
        if (!eol) break;
        size_t len = (size_t)(eol - p);
        if (!skip_header(p)) {
            if (off + len + 2 >= cap) return -1;
            memcpy(out + off, p, len);
            off += (int)len;
            out[off++] = '\r';
            out[off++] = '\n';
        }
        p = eol + 2;
    }

    char hostline[300];
    if (port == 80) snprintf(hostline, sizeof hostline, "%s", host);
    else            snprintf(hostline, sizeof hostline, "%s:%d", host, port);

    int w = snprintf(out + off, cap - off,
                     "Host: %s\r\nConnection: close\r\nAccept-Encoding: identity\r\n"
                     "Via: 1.1 cn-proxy\r\nX-Forwarded-For: %s\r\n\r\n",
                     hostline, client_ip);
    if (w < 0 || (size_t)(off + w) >= cap) return -1;
    return off + w;
}

/* Copy the origin's response to the client. Returns bytes relayed, sets *status. */
static size_t relay_response(int ofd, int cfd, int *status) {
    char buf[8192];
    size_t total = 0;
    int first = 1;
    *status = 0;

    for (;;) {
        ssize_t n = recv(ofd, buf, sizeof buf - 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;                       /* origin closed, error, or timeout */
        if (first) {
            first = 0;
            buf[n] = '\0';
            if (sscanf(buf, "HTTP/%*s %d", status) != 1) *status = 0;
        }
        if (send_all(cfd, buf, (size_t)n) < 0) break;   /* client went away */
        total += (size_t)n;
    }
    return total;
}

#define MAX_BODY (10L * 1024 * 1024)   /* refuse request bodies over 10 MB */

/* Returns a pointer to the value of header `name`, or NULL if absent. */
static const char *find_header(const char *req, const char *name) {
    size_t nl = strlen(name);
    const char *end = strstr(req, "\r\n\r\n");
    const char *p = strstr(req, "\r\n");
    if (!p || !end) return NULL;
    p += 2;
    while (p < end) {
        if (strncasecmp(p, name, nl) == 0 && p[nl] == ':') return p + nl + 1;
        const char *eol = strstr(p, "\r\n");
        if (!eol) break;
        p = eol + 2;
    }
    return NULL;
}

/* Content-Length of the request: 0 if absent, -1 if invalid. */
static long get_content_length(const char *req) {
    const char *v = find_header(req, "Content-Length");
    if (!v) return 0;
    char *endp;
    long c = strtol(v, &endp, 10);
    if (endp == v || c < 0) return -1;
    return c;
}

void handle_client(int client_fd, const char *client_ip) {
    double t0 = now_ms();
    char buf[8192];
    int n = read_headers(client_fd, buf, sizeof buf);
    if (n <= 0) return;

    char method[16], url[2048], version[16];
    if (sscanf(buf, "%15s %2047s %15s", method, url, version) != 3) {
        send_error(client_fd, 400, "Bad Request");
        log_request(client_ip, "-", "-", 400, 0, "-", now_ms() - t0);
        return;
    }
    if (strcmp(method, "CONNECT") == 0) {
        handle_connect(client_fd, client_ip, url);
        return;
    }

    char host[256], path[2048];
    int port;
    if (parse_url(url, host, &port, path) < 0) {
        send_error(client_fd, 400, "Bad Request");
        log_request(client_ip, method, url, 400, 0, "-", now_ms() - t0);
        return;
    }

    long clen = get_content_length(buf);
    if (clen < 0) {
        send_error(client_fd, 400, "Bad Request");
        log_request(client_ip, method, url, 400, 0, "-", now_ms() - t0);
        return;
    }
    if (clen > MAX_BODY) {
        send_error(client_fd, 413, "Payload Too Large");
        log_request(client_ip, method, url, 413, 0, "-", now_ms() - t0);
        return;
    }
    if (find_header(buf, "Transfer-Encoding")) {   /* chunked request bodies: not supported */
        send_error(client_fd, 501, "Not Implemented");
        log_request(client_ip, method, url, 501, 0, "-", now_ms() - t0);
        return;
    }

    int ofd = connect_to_host(host, port);
    if (ofd < 0) {
        send_error(client_fd, 502, "Bad Gateway");
        log_request(client_ip, method, url, 502, 0, "-", now_ms() - t0);
        return;
    }

    char req[9000];
    int rl = build_origin_request(req, sizeof req, method, path, host, port, client_ip, buf);
    if (rl < 0 || send_all(ofd, req, (size_t)rl) < 0) {
        close(ofd);
        send_error(client_fd, 502, "Bad Gateway");
        log_request(client_ip, method, url, 502, 0, "-", now_ms() - t0);
        return;
    }

    /* Forward the request body (POST/PUT), if any. */
    if (clen > 0) {
        const char *body_start = strstr(buf, "\r\n\r\n") + 4;
        long already = n - (long)(body_start - buf);   /* body bytes read together with headers */
        if (already > clen) already = clen;
        if (already > 0 && send_all(ofd, body_start, (size_t)already) < 0) {
            close(ofd);
            send_error(client_fd, 502, "Bad Gateway");
            log_request(client_ip, method, url, 502, 0, "-", now_ms() - t0);
            return;
        }
        long remaining = clen - (already > 0 ? already : 0);
        char bb[8192];
        while (remaining > 0) {
            size_t want = remaining > (long)sizeof bb ? sizeof bb : (size_t)remaining;
            ssize_t r = recv(client_fd, bb, want, 0);
            if (r < 0 && errno == EINTR) continue;
            if (r <= 0) {                              /* client vanished mid-upload */
                close(ofd);
                log_request(client_ip, method, url, 400, 0, "-", now_ms() - t0);
                return;
            }
            if (send_all(ofd, bb, (size_t)r) < 0) {
                close(ofd);
                send_error(client_fd, 502, "Bad Gateway");
                log_request(client_ip, method, url, 502, 0, "-", now_ms() - t0);
                return;
            }
            remaining -= r;
        }
    }

    int status;
    size_t bytes = relay_response(ofd, client_fd, &status);
    close(ofd);

    if (bytes == 0) {                            /* origin sent nothing: timeout or reset */
        send_error(client_fd, 504, "Gateway Timeout");
        status = 504;
    }
    log_request(client_ip, method, url, status, bytes, "MISS", now_ms() - t0);
}
