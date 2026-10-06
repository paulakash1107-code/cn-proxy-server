#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "proxy.h"
#include "http.h"

/* TEMPORARY: parses the request and echoes what it understood. */
void handle_client(int client_fd, const char *client_ip) {
    char buf[8192];
    int n = read_headers(client_fd, buf, sizeof buf);
    if (n <= 0) return;

    char method[16], url[2048], version[16];
    if (sscanf(buf, "%15s %2047s %15s", method, url, version) != 3) {
        send_error(client_fd, 400, "Bad Request");
        return;
    }
    if (strcmp(method, "CONNECT") == 0) {
        send_error(client_fd, 501, "CONNECT not implemented yet");
        return;
    }

    char host[256], path[2048];
    int port;
    if (parse_url(url, host, &port, path) < 0) {
        send_error(client_fd, 400, "Bad Request");
        return;
    }

    char body[3072], resp[3400];
    int bl = snprintf(body, sizeof body, "client=%s method=%s host=%s port=%d path=%s\n",
                      client_ip, method, host, port, path);
    int rl = snprintf(resp, sizeof resp,
                      "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                      bl, body);
    send_all(client_fd, resp, (size_t)rl);
}
