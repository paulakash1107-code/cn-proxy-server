#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Test helper: answers every POST with "received N bytes: <first 100 bytes>". */
static void serve(int fd) {
    char buf[65536];
    size_t total = 0;
    char *end = NULL;

    while (total < sizeof buf - 1) {
        ssize_t n = recv(fd, buf + total, sizeof buf - 1 - total, 0);
        if (n <= 0) return;
        total += (size_t)n;
        buf[total] = '\0';
        if ((end = strstr(buf, "\r\n\r\n")) != NULL) break;
    }
    if (!end) return;

    long clen = 0;
    const char *h = strcasestr(buf, "Content-Length:");
    if (h) clen = atol(h + 15);

    char preview[101];
    size_t plen = 0;
    const char *body = end + 4;
    size_t have = total - (size_t)(body - buf);
    long got = (long)have;
    if (have > 100) have = 100;
    memcpy(preview, body, have);
    plen = have;

    while (got < clen) {
        char tmp[8192];
        ssize_t n = recv(fd, tmp, sizeof tmp, 0);
        if (n <= 0) break;
        if (plen < 100) {
            size_t take = (size_t)n < 100 - plen ? (size_t)n : 100 - plen;
            memcpy(preview + plen, tmp, take);
            plen += take;
        }
        got += n;
    }
    preview[plen] = '\0';

    char msg[256], resp[512];
    int ml = snprintf(msg, sizeof msg, "received %ld bytes: %s", got, preview);
    int rl = snprintf(resp, sizeof resp,
                      "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                      ml, msg);
    send(fd, resp, (size_t)rl, MSG_NOSIGNAL);
}

int main(void) {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(9001);
    if (bind(srv, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind"); return 1; }
    listen(srv, 128);
    printf("echo server on port 9001\n");
    for (;;) {
        int fd = accept(srv, NULL, NULL);
        if (fd < 0) continue;
        serve(fd);
        close(fd);
    }
}
