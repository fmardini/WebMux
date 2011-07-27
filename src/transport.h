#ifndef _TRANSPORT_H
#define _TRANSPORT_H

#include "common.h"
#include "connection.h"

// Forward decls
struct _transport_;
struct _muxConn_;

struct _transport_ {
  int port;
  void (*xprt_initialize)(struct _transport_ *xprt);
  void (*xprt_conn_cb)(struct _muxConn_ *mc);
  void (*xprt_read_cb)(struct _muxConn_ *mc, ssize_t recved);
  void (*xprt_recv_msg)(struct _muxConn_ *mc, char *msg, int msg_len);
  void (*xprt_write_out)(struct _muxConn_ *mc, char *msg, int msg_len);
  void (*xprt_free_data)(struct _muxConn_ *mc);
};

typedef struct _transport_ transport;

int setup_transport(EV_P_ transport *xprt);

#endif
