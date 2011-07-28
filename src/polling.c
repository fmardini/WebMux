#include "polling.h"

extern http_parser_settings po_settings;

static int on_message_complete(http_parser *parser) {
  muxConn *conn = (muxConn *)parser->data;
  po_transport_data *data = conn->transport_data;
  char *msg = NULL;
  int msg_len = 0;
  // parse cookie header
  if (parser->method == HTTP_GET) {
    msg = data->req_url;
    msg_len = data->url_len;
  } else if (parser->method == HTTP_POST) {
    msg = data->body;
    msg_len = data->body_len;
  }
  msg_len = uri_unescape(msg, msg_len);
  if (msg_len < 0) { return -1; }
  conn->xprt->xprt_recv_msg(conn, msg, msg_len);
  // reset parser for further requests on same connection
  po_reset_xprt_data(data);
  return 0;
}

static void po_reset_xprt_data(po_transport_data *data) {
  http_parser_init(data->parser, HTTP_REQUEST);
  if (data->body != NULL) { free(data->body); data->body_len = 0; }
  if (data->req_url != NULL) { free(data->req_url); data->url_len = 0; }
  header *p;
  for (int i = 0; i < data->hs.num_headers; i++) {
    p = &data->hs.list[i];
    free(p->field); free(p->value);
    p->field = p->value = NULL; p->field_len = p->value_len = NULL;
  }
  for (int i = 0; i < 32; i++) {
    p = &data->hs.list[i];
    assert(p->value == NULL);
    assert(p->field == NULL);
    assert(p->field_len == 0);
    assert(p->value_len == 0);
  }
  data->hs.last_was_value = data->hs.num_headers = 0;
}

void po_initialize(transport *xprt) {
  IGNORE_VAR(xprt);
  po_settings.on_header_field     = on_header_field;
  po_settings.on_header_value     = on_header_value;
  po_settings.on_headers_complete = on_headers_complete;
  po_settings.on_body             = on_body;
  po_settings.on_url              = on_url;
  po_settings.on_message_complete = on_message_complete;
}

void po_read_cb(muxConn *mc, ssize_t recved) {
  po_transport_data *data = mc->transport_data;

  // no need to update in_buf_contents, once parsed data can be discarded
  ssize_t len_parsed;
  http_parser *parser = data->parser;
  len_parsed = http_parser_execute(parser, &po_settings, mc->in_buf, recved);

  if (len_parsed != recved) {
    perror("POLLING PARSE ERROR");
    disconnectAndClean(mc);
    return;
  }
}

void po_conn_cb(muxConn *mc) {
  po_transport_data *data = calloc(1, sizeof(po_transport_data));
  http_parser *parser = malloc(sizeof(http_parser));
  http_parser_init(parser, HTTP_REQUEST);
  parser->data = mc;
  data->parser = parser;
  mc->transport_data = data;
}

void po_free_data(muxConn *mc) {
  po_transport_data *data = mc->transport_data;
  free(data->parser);
  header p;
  for (int i = 0; i < data->hs.num_headers; i++) {
    p = data->hs.list[i];
    free(p.field); free(p.value);
  }
  if (data->req_url != NULL) { free(data->req_url); }
  if (data->body != NULL) { free(data->body); }
  free(data);
}

#define OB_GROW_IF_NEEDED(MC, LAST, CON_LEN, SOFAR) do {      \
    (PTR) += (LAST);                                    \
    (SOFAR) += (LAST);                                  \
    if ((SOFAR) >= (CUR_LEN)) { return -1; }                \
  } while (0)

void po_write_out(muxConn *mc, char *msg, int msg_len) {
  po_transport_data *data = mc->transport_data;
  char hds[512];
  int n = snprintf(hds, 512, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html; charset=utf-8\r\nServer: Jawaker\r\n\r\n", msg_len);
  int needed = mc->outBufOffset + mc->outBufToWrite + msg_len + n;
  if (mc->outBufLen < needed) {
    mc->outBufLen = needed * 1.2;
    mc->outBuf = realloc(mc->outBuf, mc->outBufLen);
  }
  char *p = mc->outBuf + mc->outBufOffset + mc->outBufToWrite;
  memcpy(p, hds, n);
  p += n;
  memcpy(p, msg, msg_len);
  mc->outBufToWrite += msg_len + n;
  if (!ev_is_active(mc->watcher)) {
# if EV_MULTIPLICITY
    ev_io_start(mc->loop, mc->watcher);
# else
    ev_io_start(mc->watcher);
# endif
  }
}

void po_recv_msg(muxConn *mc, char *msg, int msg_len) {
  write(STDOUT_FILENO, msg, msg_len);
  // echo back
  mc->xprt->xprt_write_out(mc, msg, msg_len);
}

static void process_last_header(muxConn *conn) {
  po_transport_data *data = conn->transport_data;
  header *last_header = &data->hs.list[data->hs.num_headers];
}

static int on_body(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  po_transport_data *data = conn->transport_data;
  data->body = (char *)malloc(sizeof(char) * len);
  memcpy(data->body, at, len);
  data->body_len = len;
  return 0;
}

static int on_header_field(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  po_transport_data *data = conn->transport_data;
  if (data->hs.last_was_value) {
    // last_header points to a complete header, do any processing
    process_last_header(conn);

    data->hs.num_headers++;

    CURRENT_LINE(data)->value = NULL;
    CURRENT_LINE(data)->value_len = 0;

    CURRENT_LINE(data)->field_len = len;
    CURRENT_LINE(data)->field = malloc(len+1);
    strncpy(CURRENT_LINE(data)->field, at, len);
  } else {
    assert(CURRENT_LINE(data)->value == NULL);
    assert(CURRENT_LINE(data)->value_len == 0);

    CURRENT_LINE(data)->field_len += len;
    CURRENT_LINE(data)->field = realloc(CURRENT_LINE(data)->field, CURRENT_LINE(data)->field_len + 1);
    strncat(CURRENT_LINE(data)->field, at, len);
  }

  CURRENT_LINE(data)->field[CURRENT_LINE(data)->field_len] = '\0';
  data->hs.last_was_value = 0;
  return 0;
}

int on_header_value(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  po_transport_data *data = conn->transport_data;
  if (!data->hs.last_was_value) {
    CURRENT_LINE(data)->value_len = len;
    CURRENT_LINE(data)->value = malloc(len+1);
    strncpy(CURRENT_LINE(data)->value, at, len);
  } else {
    CURRENT_LINE(data)->value_len += len;
    CURRENT_LINE(data)->value = realloc(CURRENT_LINE(data)->value, CURRENT_LINE(data)->value_len + 1);
    strncat(CURRENT_LINE(data)->value, at, len);
  }

  CURRENT_LINE(data)->value[CURRENT_LINE(data)->value_len] = '\0';
  data->hs.last_was_value = 1;
  return 0;
}

int on_headers_complete(http_parser *parser) {
  muxConn *conn = (muxConn *)parser->data;
  po_transport_data *data = conn->transport_data;
  process_last_header(conn);
  data->hs.num_headers++;
  return 0;
}

int on_url(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  po_transport_data *data = conn->transport_data;
  data->req_url = (char *)malloc(sizeof(char) * (len + 1));
  memcpy(data->req_url, at, len);
  data->req_url[len] = '\0';
  data->url_len = len;
  return 0;
}

