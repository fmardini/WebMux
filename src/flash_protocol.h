#ifndef _FLASH_PROTOCOL_H
#define _FLASH_PROTOCOL_H

#include "common.h"
#include "transport.h"
#include "net_helpers.h"
#include "adapters/libev.h"

typedef struct {
  int cur_line_start;
} fp_transport_data;

void fp_initialize(transport *xprt);
void fp_conn_cb(muxConn *mc);
void fp_read_cb(muxConn *mc, ssize_t recved);
void fp_recv_msg(muxConn *mc, char *msg, int msg_len);
void fp_write_out(muxConn *mc, char *msg, int msg_len);
void fp_free_data(muxConn *mc);


redisAsyncContext *pubContext;
static void connectCallback(const redisAsyncContext *c, int status);
void uq_redis_connect(EV_P);

#endif

