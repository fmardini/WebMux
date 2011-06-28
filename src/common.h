#ifndef _WEB_MUX_COMMON_H
#define _WEB_MUX_COMMON_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
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
  size_t in_buf_contents;
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

#endif
