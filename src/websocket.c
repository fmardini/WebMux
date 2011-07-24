#include "websocket.h"

int process_key(char *k, unsigned int *res) {
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

int compute_checksum(char *f1, char *f2, char *last8, unsigned char *out) {
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

#define CHECK_FIT_ERR(MAX, LAST, PTR, SOFAR) do {   \
    (PTR) += (LAST);                                \
    (SOFAR) += (LAST);                              \
    if ((SOFAR) >= (MAX)) { return -1; }            \
  } while (0)

int server_handshake(unsigned char *md5, char *origin, char *loc, char *protocol, char *resp, int resp_len) {
  char *p = resp;
  int len = 0, n;
  n = snprintf(p, resp_len - len, "HTTP/1.1 101 Web Socket Protocol Handshake\r\nUpgrade: WebSocket\r\nConnection: Upgrade\r\n");
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

int handshake_connection(muxConn *conn) {
  unsigned char *cksum = (unsigned char *)calloc(1, 17 * sizeof(char)); // 16 + NULL
  if (0 != compute_checksum(conn->keys[0], conn->keys[1], conn->body, cksum)) { return -1; }
  char *loc = (char *)calloc(1, (strlen(conn->host) + strlen(conn->req_path) + 1) * sizeof(char));
  sprintf(loc, "%s%s", conn->host, conn->req_path);
  char *resp = (char *)malloc(2048 * sizeof(char));
  if (0 != server_handshake(cksum, conn->origin, loc, conn->protocol, resp, 2048)) { return -1; }
  write_to_client(conn->loop, conn, 0, (unsigned char *)resp, strlen(resp));
  free(cksum); free(loc); free(resp);
  return 0;
}

void process_last_header(muxConn *conn) {
  header *last_header = &conn->hs.list[conn->hs.num_headers];
  if (strstr(last_header->field, "Sec-WebSocket-Key")) {
    int idx = *(last_header->field + strlen("Sec-WebSocket-Key")) - '1';
    if (0 <= idx && idx <= 1) { conn->keys[idx] = last_header->value; }
  } else if (strstr(last_header->field, "Origin")) {
    conn->origin = last_header->value;
  } else if (strstr(last_header->field, "Host")) {
    conn->host = last_header->value;
  } else if (strstr(last_header->field, "Sec-WebSocket-Protocol")) {
    conn->protocol = last_header->value;
  }
}

int on_header_field(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  if (conn->hs.last_was_value) {
    // last_header points to a complete header, do any processing
    process_last_header(conn);

    conn->hs.num_headers++;

    CURRENT_LINE(conn)->value = NULL;
    CURRENT_LINE(conn)->value_len = 0;

    CURRENT_LINE(conn)->field_len = len;
    CURRENT_LINE(conn)->field = malloc(len+1);
    strncpy(CURRENT_LINE(conn)->field, at, len);
  } else {
    assert(CURRENT_LINE(conn)->value == NULL);
    assert(CURRENT_LINE(conn)->value_len == 0);

    CURRENT_LINE(conn)->field_len += len;
    CURRENT_LINE(conn)->field = realloc(CURRENT_LINE(conn)->field, CURRENT_LINE(conn)->field_len + 1);
    strncat(CURRENT_LINE(conn)->field, at, len);
  }

  CURRENT_LINE(conn)->field[CURRENT_LINE(conn)->field_len] = '\0';
  conn->hs.last_was_value = 0;
  return 0;
}

int on_header_value(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  if (!conn->hs.last_was_value) {
    CURRENT_LINE(conn)->value_len = len;
    CURRENT_LINE(conn)->value = malloc(len+1);
    strncpy(CURRENT_LINE(conn)->value, at, len);
  } else {
    CURRENT_LINE(conn)->value_len += len;
    CURRENT_LINE(conn)->value = realloc(CURRENT_LINE(conn)->value, CURRENT_LINE(conn)->value_len + 1);
    strncat(CURRENT_LINE(conn)->value, at, len);
  }

  CURRENT_LINE(conn)->value[CURRENT_LINE(conn)->value_len] = '\0';
  conn->hs.last_was_value = 1;
  return 0;
}

int on_headers_complete(http_parser *parser) {
  muxConn *conn = (muxConn *)parser->data;
  process_last_header(conn);
  conn->hs.num_headers++;
  return 0;
}

int on_complete(http_parser *parser) {
  ((muxConn *)parser->data)->handshakeDone = 1;
  return 0;
}

int on_path(http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  conn->req_path = (char *)malloc(sizeof(char) * (len + 1));
  memcpy(conn->req_path, at, len);
  conn->req_path[len] = '\0';
  return 0;
}

void free_mux_conn(muxConn *conn) {
  close(conn->connfd);
  if (conn->user_id != NULL) free(conn->user_id);
  ev_io_stop(conn->loop, conn->watcher); ev_io_stop(conn->loop, conn->read_watcher);
  free(conn->watcher); free(conn->read_watcher);
  if (conn->updates_watcher != NULL) {
    ev_timer_stop(conn->loop, conn->updates_watcher); free(conn->updates_watcher);
  }
  if (conn->parser != NULL) free(conn->parser);
  header p;
  for (int i = 0; i < conn->hs.num_headers; i++) {
    p = conn->hs.list[i];
    free(p.field); free(p.value);
  }
  if (conn->req_path != NULL) { free(conn->req_path); }
  free(conn->in_buf);
  free(conn->outBuf);
  free(conn);
}
