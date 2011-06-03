#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <openssl/md5.h>

#include "../deps/http-parser/http_parser.h"

int set_nonblock (int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    return -1;
  return 0;
}

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
  int connfd;
  int reqDone;
  char body[8];
  char *req_path;
  char *keys[2]; // points to value in headers list
  char *origin; // points to value in headers list
  char *host; // points to value in headers list
  char *protocol; // points to value in headers list
  headers hs;
} muxConn;

void free_mux_conn(muxConn *conn) {
  free(conn->req_path);
  int i;
  header p;
  printf("num headers = %d\n", conn->hs.num_headers);
  for (i = 0; i < conn->hs.num_headers; i++) {
    p = conn->hs.list[i];
    free(p.field); free(p.value);
  }
  free(conn);
}


void check_error(int res, char *msg) {
  if (res >= 0) return;
  fprintf(stderr, "Error (%s): %s\n", msg, strerror(errno));
}

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

int server_handshake(unsigned char *md5, char *origin, char *loc, char *protocol, char *resp, int resp_len) {
  char *p = resp;
  int len = 0, n;
  n = snprintf(p, resp_len - len, "HTTP/1.1 101 Web Socket Protocol Handshake\r\nUpgrade: WebSocket\r\nConnection: Upgrade\r\n");
  p += n; len += n;
  n = snprintf(p, resp_len - len, "Sec-WebSocket-Origin: %s\r\n", origin);
  p += n; len += n;
  n = snprintf(p, resp_len - len, "Sec-WebSocket-Location: ws://%s\r\n", loc);
  p += n; len += n;
  if (protocol != NULL) {
    n = snprintf(p, resp_len - len, "Sec-WebSocket-Protocal: %s\r\n", protocol);
    p += n; len += n;
  }
  n = snprintf(p, resp_len - len, "\r\n%s", md5);
  p += n; len += n;
  resp[len] = '\0';
  return 0;
}

int handshake_connection(muxConn *conn) {
  unsigned char *cksum = (unsigned char *)calloc(1, 17 * sizeof(char)); // 16 + NULL
  compute_checksum(conn->keys[0], conn->keys[1], conn->body, cksum);
  char *loc = (char *)calloc(1, (strlen(conn->host) + strlen(conn->req_path)) * sizeof(char));
  sprintf(loc, "%s%s", conn->host, conn->req_path);
  char *resp = (char *)malloc(2048 * sizeof(char));
  server_handshake(cksum, conn->origin, loc, conn->protocol, resp, 2048);
  write(conn->connfd, resp, strlen(resp));
  free(cksum); free(loc); free(resp);
  return 0;
}

#define CURRENT_LINE(_mc) (&(_mc)->hs.list[(_mc)->hs.num_headers])

void process_last_header(muxConn *conn) {
  header *last_header = &conn->hs.list[conn->hs.num_headers];
  printf("HEADER ===> %s : %s\n", last_header->field, last_header->value);
  if (strstr(last_header->field, "Sec-WebSocket-Key")) {
    int idx = *(last_header->field + strlen("Sec-WebSocket-Key")) - '1';
    printf("idx = %d\n", idx);
    if (0 <= idx && idx <= 1) { conn->keys[idx] = last_header->value; }
  } else if (strstr(last_header->field, "Origin")) {
    conn->origin = last_header->value;
  } else if (strstr(last_header->field, "Host")) {
    conn->host = last_header->value;
  } else if (strstr(last_header->field, "Sec-WebSocket-Protocol")) {
    conn->protocol = last_header->value;
  }
}

int on_header_field (http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  if (conn->hs.last_was_value) {
    // last_header points to a complete header, do any processing
    process_last_header(conn);

    // handle next
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

int on_header_value (http_parser *parser, const char *at, size_t len) {
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

int on_complete(http_parser *parser) {
  ((muxConn *)parser->data)->reqDone = 1;
  return 0;
}

int on_headers_complete(http_parser *parser) {
  muxConn *conn = (muxConn *)parser->data;
  process_last_header(conn);
  conn->hs.num_headers++;
  return 0;
}

int on_path(http_parser *parser, const char *at, size_t len) {
  ((muxConn *)parser->data)->req_path = (char *)malloc(sizeof(char) * len);
  memcpy(((muxConn *)parser->data)->req_path, at, len);
  return 0;
}


int main(int argc, char **argv) {
  int listenfd;
  struct sockaddr_in servaddr, cliaddr;
  socklen_t len = sizeof(cliaddr);

  memset(&servaddr, 0, sizeof(servaddr));
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  int optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  check_error(listenfd, "listen");
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(4000); /* daytime server */
  check_error(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)), "bind");
  check_error(listen(listenfd, 128), "listen");
  int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
  check_error(connfd, "accept");

  http_parser_settings settings;
  muxConn *conn = (muxConn *)calloc(1, sizeof(muxConn)); // ZEROED-OUT
  settings.on_header_field = on_header_field;
  settings.on_header_value = on_header_value;
  settings.on_message_complete = on_complete;
  settings.on_headers_complete = on_headers_complete;
  settings.on_path = on_path;
  http_parser *parser = (http_parser *)malloc(sizeof(http_parser));
  http_parser_init(parser, HTTP_REQUEST);
  parser->data = conn;

  conn->connfd = connfd;
  char buf[1024 * 4];
  ssize_t recved;
  ssize_t len_parsed;

  while (!conn->reqDone) {
    recved = recv(connfd, buf, 4 * 1024, 0);
    len_parsed = http_parser_execute(parser, &settings, buf, recved);

    if (parser->upgrade) {
      // the +1 is needed since in this case buf[len_parsed] is the \n, BUG??
      if (recved - len_parsed - 1 == 8) {
        memcpy(conn->body, buf + len_parsed + 1, recved - len_parsed - 1);
      } else
        fprintf(stderr, "invalid body length");
    } else if (len_parsed != recved)
      fprintf(stderr, "PARSE ERROR\n");
  }

  printf("k1: %s\n", conn->keys[0]);
  printf("k2: %s\n", conn->keys[1]);
  printf("host: %s\n", conn->host);
  printf("origin: %s\n", conn->origin);

  if (conn->keys[0] != NULL) {
    handshake_connection(conn);
  } else {
    fprintf(stderr, "could not find keys\n");
  }

  char t[1]; read(STDIN_FILENO, t, 1);
  free_mux_conn(conn);
  free(parser);
  close(connfd);
  close(listenfd);

  return 0;
}
