#include <string.h>
#include <sys/socket.h>
#include "proxy.h"

/* TEMPORARY: replies with a fixed message. Real forwarding comes in the next step. */
void handle_client(int client_fd, const char *client_ip) {
    (void)client_ip;
    const char *msg = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\nproxy alive\n";
    send(client_fd, msg, strlen(msg), MSG_NOSIGNAL);
}
