#ifndef _MUX_POLLING_H_
#define _MUX_POLLING_H_

#include "common.h"
#include "transport.h"
#include "http_helpers.h"

typedef struct {
  http_state *st;
  char *body;
  int body_len;
  char *req_url;
  int url_len;
} po_transport_data;


void po_initialize(transport *xprt);
void po_conn_cb(muxConn *mc);
void po_read_cb(muxConn *mc, ssize_t recved);
void po_recv_msg(muxConn *mc, char *msg, int msg_len);
void po_write_out(muxConn *mc, char *msg, int msg_len);
void po_free_data(muxConn *mc);

static int on_body(http_parser *parser, const char *at, size_t len);
static int on_url(http_parser *parser, const char *at, size_t len);
static void po_reset_xprt_data(po_transport_data *data);

http_parser_settings po_settings;

#endif
