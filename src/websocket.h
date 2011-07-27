#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

#include "common.h"
#include "transport.h"
#include <openssl/md5.h>
#include "http_parser.h"

#define MAX_HEADERS 32

typedef struct header {
  char   *field;
  size_t field_len;
  char   *value;
  size_t value_len;
} header;

typedef struct {
  header list[MAX_HEADERS];
  int last_was_value;
  int num_headers;
} headers;

typedef struct {
  http_parser *parser;
  headers hs;
  char body[8];
  char *req_path;
  char *keys[2];  // points to value in headers list
  char *origin;   // points to value in headers list
  char *host;     // points to value in headers list
  char *protocol; // points to value in headers list
  int websocket_in_frame;
  int cur_frame_start;
  int handshakeDone;
} ws_transport_data;


#define CURRENT_LINE(_mc) (&(_mc)->hs.list[(_mc)->hs.num_headers])

void ws_initialize(transport *xprt);
void ws_conn_cb(muxConn *mc);
void ws_read_cb(muxConn *mc, ssize_t recved);
void ws_recv_msg(muxConn *mc, char *msg, int msg_len);
void ws_write_out(muxConn *mc, char *msg, int msg_len);
void ws_free_data(muxConn *mc);


static int process_key(char *k, unsigned int *res);
static int compute_checksum(char *f1, char *f2, char *last8, unsigned char *out);
static int server_handshake(unsigned char *md5, char *origin, char *loc, char *protocol, char *resp, int resp_len);
static int handshake_connection(muxConn *conn);
static void process_last_header(muxConn *conn);
static int on_header_field(http_parser *parser, const char *at, size_t len);
static int on_header_value(http_parser *parser, const char *at, size_t len);
static int on_headers_complete(http_parser *parser);
static int on_url(http_parser *parser, const char *at, size_t len);

http_parser_settings settings;


#endif
