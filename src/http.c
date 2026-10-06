#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include "http.h"

/* Read until the blank line that ends the headers. */
int read_headers(int fd, char *buf, size_t cap) {
    size_t total = 0;
    while (total < cap - 1) {
        ssize_t n = recv(fd, buf + total, cap - 1 - total, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;              /* closed, error or timeout */
        total += (size_t)n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) return (int)total;
    }
    return -1;                              /* headers too large */
}

/* "http://host:port/path" -> host, port, path */
int parse_url(const char *url, char *host, int *port, char *path) {
    if (strncmp(url, "http://", 7) != 0) return -1;
    const char *p = url + 7;
    const char *slash = strchr(p, '/');
    size_t authlen = slash ? (size_t)(slash - p) : strlen(p);
    if (authlen == 0 || authlen >= 256) return -1;

    char auth[256];
    memcpy(auth, p, authlen);
    auth[authlen] = '\0';

    *port = 80;
    char *colon = strrchr(auth, ':');
    if (colon) {
        *colon = '\0';
        int pt = atoi(colon + 1);
        if (pt <= 0 || pt > 65535) return -1;
        *port = pt;
    }
    if (auth[0] == '\0') return -1;
    strcpy(host, auth);                     /* safe: auth < 256 bytes */

    if (slash) {
        if (strlen(slash) >= 2048) return -1;
        strcpy(path, slash);
    } else {
        strcpy(path, "/");
    }
    return 0;
}

/* send() can write only part of the data, so loop until everything is out. */
int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

void send_error(int fd, int code, const char *msg) {
    char body[256], resp[640];
    int bl = snprintf(body, sizeof body,
                      "<html><body><h1>%d %s</h1></body></html>\n", code, msg);
    int rl = snprintf(resp, sizeof resp,
                      "HTTP/1.1 %d %s\r\nContent-Type: text/html\r\n"
                      "Content-Length: %d\r\nConnection: close\r\n\r\n%s",
                      code, msg, bl, body);
    if (rl > 0 && (size_t)rl < sizeof resp) send_all(fd, resp, (size_t)rl);
}
