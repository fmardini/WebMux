#include "polling.h"

extern http_parser_settings po_settings;

static int on_message_complete(http_parser *parser) {
  http_state *st = parser->data;
  muxConn *conn = st->mc;
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
  http_state *st = data->st;
  http_parser_init(st->parser, HTTP_REQUEST);
  free_headers(st);
  if (data->body != NULL) { free(data->body); data->body_len = 0; }
  if (data->req_url != NULL) { free(data->req_url); data->url_len = 0; }
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
  http_parser *parser = data->st->parser;
  len_parsed = http_parser_execute(parser, &po_settings, mc->in_buf, recved);

  if (len_parsed != recved) {
    perror("POLLING PARSE ERROR");
    disconnectAndClean(mc);
    return;
  }
}

void po_conn_cb(muxConn *mc) {
  po_transport_data *data = calloc(1, sizeof(po_transport_data));
  data->st = init_http_state(mc, NULL);
  mc->transport_data = data;
}

void po_free_data(muxConn *mc) {
  po_transport_data *data = mc->transport_data;
  free_http_state(data->st);
  if (data->req_url != NULL) { free(data->req_url); }
  if (data->body != NULL) { free(data->body); }
  free(data);
}

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

static int on_body(http_parser *parser, const char *at, size_t len) {
  http_state *st          = parser->data;
  muxConn *mc             = st->mc;
  po_transport_data *data = mc->transport_data;
  data->body              = malloc(len + 1);
  data->body_len          = len;
  memcpy(data->body, at, len);
  data->body[len]         = '\0';
  return 0;
}

int on_url(http_parser *parser, const char *at, size_t len) {
  http_state *st          = parser->data;
  muxConn *mc             = st->mc;
  po_transport_data *data = mc->transport_data;
  data->req_url           = malloc(len + 1);
  data->url_len           = len;
  memcpy(data->req_url, at, len);
  data->req_url[len]      = '\0';
  return 0;
}

