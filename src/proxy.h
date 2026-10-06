#ifndef PROXY_H
#define PROXY_H

/* Handles one client connection from start to finish (runs in its own thread). */
void handle_client(int client_fd, const char *client_ip);
#endif
