#ifndef _MUX_POLLING_H_
#define _MUX_POLLING_H_

#include "common.h"
#include "transport.h"
#include <openssl/md5.h>
#include "http_parser.h"

typedef struct {
  http_parser *parser;
  headers hs;
  char *body;
  int body_len;
  char *req_url;
  int url_len;
} po_transport_data;


#define CURRENT_LINE(_mc) (&(_mc)->hs.list[(_mc)->hs.num_headers])

void po_initialize(transport *xprt);
void po_conn_cb(muxConn *mc);
void po_read_cb(muxConn *mc, ssize_t recved);
void po_recv_msg(muxConn *mc, char *msg, int msg_len);
void po_write_out(muxConn *mc, char *msg, int msg_len);
void po_free_data(muxConn *mc);

static void process_last_header(muxConn *conn);
static int on_body(http_parser *parser, const char *at, size_t len);
static int on_header_field(http_parser *parser, const char *at, size_t len);
static int on_header_value(http_parser *parser, const char *at, size_t len);
static int on_headers_complete(http_parser *parser);
static int on_url(http_parser *parser, const char *at, size_t len);
static void po_reset_xprt_data(po_transport_data *data);

http_parser_settings po_settings;

#endif
