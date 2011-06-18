#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <assert.h>
#include <ev.h>
#include <openssl/md5.h>
#include "http_parser.h"

#define MAX_HEADERS 32

typedef struct header {
  char *field;
  size_t field_len;
  char *value;
  size_t value_len;
} header;

typedef struct {
  header list[MAX_HEADERS];
  int last_was_value;
  int num_headers;
} headers;

typedef struct muxConn {
  int connfd;
  headers hs;
  struct ev_loop *loop;
  char body[8];
  char *req_path;
  char *keys[2]; // points to value in headers list
  char *origin; // points to value in headers list
  char *host; // points to value in headers list
  char *protocol; // points to value in headers list
  char *in_buf;
  size_t in_buf_len;
  ssize_t in_buf_contents;
  int handshakeDone;
  int websocket_in_frame;
  int cur_frame_start;
  http_parser *parser;
  ev_io *watcher;
  ev_io *read_watcher;
  char *outBuf;
  int outBufLen;
  int outBufOffset;
  int outBufToWrite;
  char *connKey;
} muxConn;

// Forward declarations
int write_to_client(EV_P_ muxConn *mc, int add_frame, unsigned char *msg, size_t msg_len);

int process_key(char *k, unsigned int *res);
int compute_checksum(char *f1, char *f2, char *last8, unsigned char *out);
int server_handshake(unsigned char *md5, char *origin, char *loc, char *protocol, char *resp, int resp_len);
void free_mux_conn(muxConn *conn);
int handshake_connection(muxConn *conn);

#define CURRENT_LINE(_mc) (&(_mc)->hs.list[(_mc)->hs.num_headers])

void process_last_header(muxConn *conn);
int on_header_field(http_parser *parser, const char *at, size_t len);
int on_header_value(http_parser *parser, const char *at, size_t len);
int on_headers_complete(http_parser *parser);
int on_complete(http_parser *parser);
int on_path(http_parser *parser, const char *at, size_t len);

#endif
