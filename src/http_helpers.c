#include "http_helpers.h"

static header *init_header() {
  header *h = calloc(1, sizeof(header));
  return h;
}

http_state *init_http_state(muxConn *mc, void (*header_cb)(http_parser *parser, header *hd)) {
  http_parser *parser = malloc(sizeof(http_parser));
  http_parser_init(parser, HTTP_REQUEST);
  parser->data = mc;

  http_state *st = calloc(1, sizeof(http_state));
  st->mc = mc;
  st->parser = parser;
  st->header_cb = header_cb;

  parser->data = st;
  return st;
}

void free_headers(http_state *st) {
  header *p = st->headers, *q;
  while (p) {
    q = p;
    p = p->next;
    free(q->field); free(q->value);
    free(q);
  }
  st->headers = st->last_header = NULL;
  st->num_headers = st->last_header_cb_was_value = 0;
}

void free_http_state(http_state *st) {
  free(st->parser);
  free_headers(st);
  free(st);
}

int on_header_field(http_parser *parser, const char *at, size_t len) {
  http_state *st = parser->data;
  if (st->last_header_cb_was_value) {
    if (st->header_cb != NULL) {
      st->header_cb(st);
    }
    st->last_header->next = init_header();
    st->last_header = st->last_header->next;
    st->num_headers++;
    if (st->num_headers > MAX_HEADERS) {
      disconnectAndClean(st->mc);
    }

    st->last_header->field_len = len;
    st->last_header->field = malloc(len + 1);
    strncpy(st->last_header->field, at, len);
  } else {
    if (st->headers == NULL) {
      st->headers = init_header();
      st->last_header = st->headers;
    }
    int old_len = st->last_header->field_len;
    st->last_header->field_len += len;
    st->last_header->field = realloc(st->last_header->field, st->last_header->field_len + 1);
    strncpy(st->last_header->field + old_len, at, len);
  }

  st->last_header_cb_was_value = 0;
  return 0;
}

int on_header_value(http_parser *parser, const char *at, size_t len) {
  http_state *st = parser->data;
  if (!st->last_header_cb_was_value) {
    st->last_header->value_len = len;
    st->last_header->value = malloc(len + 1);
    strncpy(st->last_header->value, at, len);
  } else {
    int old_len = st->last_header->value_len;
    st->last_header->value_len += len;
    st->last_header->value = realloc(st->last_header->value, st->last_header->value_len + 1);
    strncpy(st->last_header->value + old_len, at, len);
  }

  st->last_header_cb_was_value = 1;
  return 0;
}

int on_headers_complete(http_parser *parser) {
  http_state *st = parser->data;
  if (st->header_cb != NULL) {
    st->header_cb(st);
  }
  st->num_headers++;
  return 0;
}

