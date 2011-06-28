#ifndef _CALLBACKS_H
#define _CALLBACKS_H

#include "common.h"
#include "websocket.h"
#include "net_helpers.h"

void client_write_cb(EV_P_ ev_io *w, int revents);
void client_read_cb(EV_P_ ev_io *w, int revents);
void listening_socket_cb(EV_P_ ev_io *w, int revents);
void shutdown_server(EV_P_ ev_signal *w, int revents);
void sigpipe_cb(EV_P_ ev_signal *w, int revents);

int write_to_client(EV_P_ muxConn *mc, int add_frame, unsigned char *msg, size_t msg_len);
void disconnectAndClean(EV_P_ ev_io *w);

#endif
