#ifndef HTTP_H
#define HTTP_H
#include <stddef.h>

/* Reads from fd until "\r\n\r\n". Returns bytes read, or <= 0 on error/timeout. */
int  read_headers(int fd, char *buf, size_t cap);

/* Splits "http://host:port/path". host buffer >= 256, path buffer >= 2048.
   Returns 0 on success, -1 on bad URL. Default port 80, default path "/". */
int  parse_url(const char *url, char *host, int *port, char *path);

/* Loops until every byte is sent. Returns 0 on success, -1 on error. */
int  send_all(int fd, const char *buf, size_t len);

/* Sends a minimal HTTP error response, e.g. send_error(fd, 403, "Forbidden"). */
void send_error(int fd, int code, const char *msg);
#endif
