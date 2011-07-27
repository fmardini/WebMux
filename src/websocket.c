#include "websocket.h"

extern http_parser_settings settings;

void ws_initialize(transport *xprt) {
  IGNORE_VAR(xprt);
  settings.on_header_field     = on_header_field;
  settings.on_header_value     = on_header_value;
  settings.on_headers_complete = on_headers_complete;
  settings.on_url              = on_url;
}

void ws_read_cb(muxConn *mc, ssize_t recved) {
  ws_transport_data *data = mc->transport_data;

  if (!data->handshakeDone) {
    // no need to update in_buf_contents, once parsed data can be discarded
    ssize_t len_parsed;
    http_parser *parser = data->parser;
    len_parsed = http_parser_execute(parser, &settings, mc->in_buf, recved);

    if (parser->upgrade) {
      if (recved - len_parsed == 8) {
        memcpy(data->body, mc->in_buf + len_parsed, 8);
      } else {
        perror("INVALID WEBSOKCET HANDSHAKE BODY LENGTH");
        disconnectAndClean(mc);
        return;
      }
    } else if (len_parsed != recved) {
      perror("WEBSOCKET HANDSHAKE PARSE ERROR");
      disconnectAndClean(mc);
      return;
    }
    if (handshake_connection(mc) != 0) {
      perror("INVALID CLIENT HANDSHAKE\n");
      disconnectAndClean(mc);
      return;
    }
  } else {
    char *p = mc->in_buf + mc->in_buf_contents;
    for (int i = 0; i < recved; i++, p++) {
      if (*(unsigned char *)p == 0x00) {
        data->cur_frame_start = p - mc->in_buf;
        data->websocket_in_frame = 1;
      } else if (*(unsigned char *)p == 0xFF) {
        mc->xprt->xprt_recv_msg(mc, mc->in_buf + data->cur_frame_start + 1, p - 1 - (mc->in_buf + data->cur_frame_start));
        data->websocket_in_frame = 0;
      }
    }
    if (!data->websocket_in_frame) {
      mc->in_buf_contents = 0;
    } else {
      mc->in_buf_contents += recved;
    }
  }
}

void ws_conn_cb(muxConn *mc) {
  ws_transport_data *data = calloc(1, sizeof(ws_transport_data));
  http_parser *parser = malloc(sizeof(http_parser));
  http_parser_init(parser, HTTP_REQUEST);
  parser->data = mc;
  data->parser = parser;
  mc->transport_data = data;
}

void ws_free_data(muxConn *mc) {
  ws_transport_data *data = mc->transport_data;
  free(data->parser);
  header p;
  for (int i = 0; i < data->hs.num_headers; i++) {
    p = data->hs.list[i];
    free(p.field); free(p.value);
  }
  if (data->req_path != NULL) { free(data->req_path); }
  free(data);
}

void ws_write_out(muxConn *mc, char *msg, int msg_len) {
  ws_transport_data *data = mc->transport_data;
  int needed = mc->outBufOffset + mc->outBufToWrite + msg_len;
  // grow output buffer if needed
  if (mc->outBufLen < needed) { mc->outBuf = realloc(mc->outBuf, needed * 1.2); }
  int p = mc->outBufOffset + mc->outBufToWrite;
  int add_frame = data->handshakeDone;
  if (add_frame) mc->outBuf[p] = '\x00';
  memcpy(mc->outBuf + p + (add_frame ? 1 : 0), msg, msg_len);
  if (add_frame) mc->outBuf[p + 1 + msg_len] = '\xFF';
  mc->outBufToWrite += msg_len + (add_frame ? 2 : 0);
  if (!ev_is_active(mc->watcher)) {
# if EV_MULTIPLICITY
    ev_io_start(mc->loop, mc->watcher);
# else
    ev_io_start(mc->watcher);
# endif
  }
}

void ws_recv_msg(muxConn *mc, char *msg, int msg_len) {
  write(STDOUT_FILENO, msg, msg_len);
}

static int process_key(char *k, unsigned int *res) {
  int j = 0, sp = 0;
  char digits[32];
  for (; *k; k++) {
    if (*k == ' ') sp++;
    else if (isdigit(*k)) { digits[j++] = *k; }
  }
  digits[j] = '\0';
  if (sp == 0 || j == 0) { return -1; }
  *res = strtoul(digits, NULL, 10) / sp;
  return 0;
}

static int compute_checksum(char *f1, char *f2, char *last8, unsigned char *out) {
  unsigned int k1, k2;
  if (process_key(f1, &k1) == -1 || process_key(f2, &k2) == -1) { return -1; }
  k1 = htonl(k1); k2 = htonl(k2);
  unsigned char kk[16];
  memcpy(kk, &k1, 4);
  memcpy(kk + 4, &k2, 4);
  memcpy(kk + 8, last8, 8);
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, kk, 16);
  MD5_Final(out, &ctx);
  return 0;
}

#define CHECK_FIT_ERR(MAX, LAST, PTR, SOFAR) do {       \
    (PTR) += (LAST);                                    \
    (SOFAR) += (LAST);                                  \
    if ((SOFAR) >= (MAX)) { return -1; }                \
  } while (0)

static int server_handshake(unsigned char *md5, char *origin, char *loc, char *protocol, char *resp, int resp_len) {
  char *p = resp;
  int len = 0, n;
  n = snprintf(p, resp_len - len, "HTTP/1.1 101 WebSocket Protocol Handshake\r\nUpgrade: WebSocket\r\nConnection: Upgrade\r\n");
  CHECK_FIT_ERR(resp_len, n, p, len);
  n = snprintf(p, resp_len - len, "Sec-WebSocket-Origin: %s\r\n", origin);
  CHECK_FIT_ERR(resp_len, n, p, len);
  n = snprintf(p, resp_len - len, "Sec-WebSocket-Location: ws://%s\r\n", loc);
  CHECK_FIT_ERR(resp_len, n, p, len);
  if (protocol != NULL) {
    n = snprintf(p, resp_len - len, "Sec-WebSocket-Protocal: %s\r\n", protocol);
    CHECK_FIT_ERR(resp_len, n, p, len);
  }
  n = snprintf(p, resp_len - len, "\r\n%s", md5);
  CHECK_FIT_ERR(resp_len, n, p, len);
  resp[len] = '\0';
  return 0;
}

static int handshake_connection(muxConn *conn) {
  unsigned char *cksum = (unsigned char *)calloc(1, 17 * sizeof(char)); // 16 + NULL
  ws_transport_data *data = conn->transport_data;
  if (0 != compute_checksum(data->keys[0], data->keys[1], data->body, cksum)) { return -1; }
  char *loc = (char *)calloc(1, (strlen(data->host) + strlen(data->req_path) + 1) * sizeof(char));
  sprintf(loc, "%s%s", data->host, data->req_path);
  char *resp = (char *)malloc(2048 * sizeof(char));
  if (0 != server_handshake(cksum, data->origin, loc, data->protocol, resp, 2048)) { return -1; }
  ws_write_out(conn, resp, strlen(resp));
  data->handshakeDone = 1;
  free(cksum); free(loc); free(resp);
  return 0;
}

static void process_last_header(muxConn *conn) {
  ws_transport_data *data = conn->transport_data;
  header *last_header = &data->hs.list[data->hs.num_headers];
  if (strstr(last_header->field, "Sec-WebSocket-Key")) {
    int idx = *(last_header->field + strlen("Sec-WebSocket-Key")) - '1';
    if (0 <= idx && idx <= 1) { data->keys[idx] = last_header->value; }
  } else if (strstr(last_header->field, "Origin")) {
    data->origin = last_header->value;
  } else if (strstr(last_header->field, "Host")) {
    data->host = last_header->value;
  } else if (strstr(last_header->field, "Sec-WebSocket-Protocol")) {
    data->protocol = last_header->value;
  }
}

static int on_header_field(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  ws_transport_data *data = conn->transport_data;
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
  ws_transport_data *data = conn->transport_data;
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
  ws_transport_data *data = conn->transport_data;
  process_last_header(conn);
  data->hs.num_headers++;
  return 0;
}

int on_url(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  ws_transport_data *data = conn->transport_data;
  data->req_path = (char *)malloc(sizeof(char) * (len + 1));
  memcpy(data->req_path, at, len);
  data->req_path[len] = '\0';
  return 0;
}

