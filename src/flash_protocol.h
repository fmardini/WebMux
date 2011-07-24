#ifndef _FLASH_PROTOCOL_H
#define _FLASH_PROTOCOL_H

#include "common.h"
#include "callbacks.h"
#include "net_helpers.h"
#include "adapters/libev.h"

void flash_protocol_read_cb(EV_P_ ev_io *w, int revents);
void flash_protocol_conn_cb(EV_P_ ev_io *w, int revents);
int flash_protocol_socket_fd(void);

redisAsyncContext *pubContext;
static void connectCallback(const redisAsyncContext *c, int status);
void uq_redis_connect(EV_P);

#endif
