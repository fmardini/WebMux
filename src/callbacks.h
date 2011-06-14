#ifndef _CALLBACKS_H
#define _CALLBACKS_H

#include "websocket.h"
#include "net_helpers.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#include "../deps/hiredis/dict.c"

void client_write_cb(EV_P_ ev_io *w, int revents);
void client_read_cb(EV_P_ ev_io *w, int revents);
void listening_socket_cb(EV_P_ ev_io *w, int revents);
void write_to_client(EV_P_ muxConn *mc, unsigned char *msg, size_t msg_len);
void disconnectAndClean(EV_P_ ev_io *w);

#endif
