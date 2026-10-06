#ifndef TUNNEL_H
#define TUNNEL_H

/* Handles "CONNECT host:port": opens a TCP tunnel and relays bytes both ways. */
void handle_connect(int client_fd, const char *client_ip, const char *target);
#endif
