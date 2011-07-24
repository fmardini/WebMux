#ifndef _WEB_MUX_COMMON_H
#define _WEB_MUX_COMMON_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <ev.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "../deps/http-parser/http_parser.h"
#include "../deps/hiredis/dict.c"

#define IGNORE_VAR(x) ((void) (x))

#define TRY_OR_EXIT(STATUS, CALL, MSG)       \
  do {                                       \
    STATUS = (CALL);                         \
    if (STATUS < 0) {                        \
      perror((MSG));                         \
      exit(1);                               \
    }                                        \
  } while (0)

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

typedef struct {
  int accept_fd;
  unsigned int uniq_id;
} accept_watcher_data;

typedef struct muxConn {
  unsigned int conn_id;
  int connfd;
  char *user_id;
  struct ev_loop *loop;
  ev_io *watcher;
  ev_io *read_watcher;
  ev_timer *updates_watcher;
  time_t timestamp;
  // WebSockets
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
  // IO
  char *in_buf;
  size_t in_buf_len;
  size_t in_buf_contents;
  char *outBuf;
  int outBufLen;
  int outBufOffset;
  int outBufToWrite;
  // FSM
  int handshakeDone; // THIS SHOULD BE PART OF THE TRANSPORT
} muxConn;

dict *active_connections;

#endif
