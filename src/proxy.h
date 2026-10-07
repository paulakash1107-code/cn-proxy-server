#ifndef PROXY_H
#define PROXY_H

/* Handles one client connection from start to finish (runs in its own thread). */
void handle_client(int client_fd, const char *client_ip);

/* Opens a TCP connection to host:port. Returns fd, or -1 on failure. */
int connect_to_host(const char *host, int port);
#endif
