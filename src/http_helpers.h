#ifndef _MUX_HTTP_HELPERS_H_
#define _MUX_HTTP_HELPERS_H_

#include "common.h"
#include "connection.h"
#include "http_parser.h"

#define MAX_HEADERS 32

typedef struct _hd {
  char   *field;
  size_t field_len;
  char   *value;
  size_t value_len;
  struct _hd *next;
} header;

static header *init_header(void);

typedef struct _st {
  muxConn *mc;
  http_parser *parser;
  header *headers;
  header *last_header;
  int last_header_cb_was_value;
  int num_headers;
  void (*header_cb)(struct _st *st);
} http_state;

http_state *init_http_state(muxConn *mc, void (*header_cb)(http_parser *parser, header *hd));
void free_headers(http_state *st);
void free_http_state(http_state *st);

int on_header_field(http_parser *parser, const char *at, size_t len);
int on_header_value(http_parser *parser, const char *at, size_t len);
int on_headers_complete(http_parser *parser);

#endif
