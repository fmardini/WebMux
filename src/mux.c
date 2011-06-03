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
  char *host;
  char *origin;
  char *req_path;
  header *last_header;
  headers hs;
} muxConn;


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

unsigned char *compute_handshake(char *f1, char *f2, char *last8) {
  unsigned int k1, k2;
  if (process_key(f1, &k1) == -1 || process_key(f2, &k2) == -1) { fprintf(stderr, "invalid keys\n"); }
  k1 = htonl(k1); k2 = htonl(k2);
  unsigned char kk[16];
  memcpy(kk, &k1, 4);
  memcpy(kk + 4, &k2, 4);
  memcpy(kk + 8, last8, 8);
  unsigned char *out = (unsigned char *)calloc(1, 17 * sizeof(char)); // 16 + NULL
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, kk, 16);
  MD5_Final(out, &ctx);
  return out;
}

char *server_handshake(unsigned char *md5, char *origin, char *loc, char *protocol) {
  char *resp = (char *)malloc(1024 * sizeof(char));
  char *p = resp;
  int len = 0, n;
  n = snprintf(p, 1024 - len, "HTTP/1.1 101 Web Socket Protocol Handshake\r\nUpgrade: WebSocket\r\nConnection: Upgrade\r\n");
  p += n; len += n;
  n = snprintf(p, 1024 - len, "Sec-WebSocket-Origin: %s\r\n", origin);
  p += n; len += n;
  n = snprintf(p, 1024 - len, "Sec-WebSocket-Location: ws://%s\r\n", loc);
  p += n; len += n;
  if (protocol != NULL) {
    n = snprintf(p, 1024 - len, "Sec-WebSocket-Protocal: %s\r\n", protocol);
    p += n; len += n;
  }
  n = snprintf(p, 1024 - len, "\r\n%s", md5);
  p += n; len += n;
  resp[len] = '\0';
  return resp;
}

#define CURRENT_LINE(_mc) (&(_mc)->hs.list[(_mc)->hs.num_headers])

int on_header_field (http_parser *parser, const char *at, size_t len) {
  muxConn *conn = (muxConn *)parser->data;
  if (conn->hs.last_was_value) {
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
      memcpy(conn->body, buf + len_parsed + 1, recved - len_parsed - 1);
    } else if (len_parsed != recved)
      fprintf(stderr, "PARSE ERROR\n");
  }

  int i;
  char *keys[2], *p, *origin, *host;
  header *headers = conn->hs.list;
  for (i = 0; i <= conn->hs.num_headers; i++) {
    printf("HEADER ===> %s : %s\n", headers[i].field, headers[i].value);
    if ((p = strstr(headers[i].field, "Sec-WebSocket-Key"))) {
      keys[*(p + strlen("Sec-WebSocket-Key")) - '1'] = headers[i].value;
    } else if ((p = strstr(headers[i].field, "Origin"))) {
      origin = headers[i].value;
    } else if ((p = strstr(headers[i].field, "Host"))) {
      host = headers[i].value;
    }
  }
  printf("k1: %s\n", keys[0]);
  printf("k2: %s\n", keys[1]);
  printf("host: %s\n", host);
  printf("origin: %s\n", origin);

  if (p != NULL) {
    unsigned char *hand_shake = compute_handshake(keys[0], keys[1], conn->body);
    char loc[256];
    snprintf(loc, 256, "%s%s", host, conn->req_path);
    char *resp = server_handshake(hand_shake, origin, loc, NULL);
    write(connfd, resp, strlen(resp));
  } else {
    fprintf(stderr, "could not find keys\n");
  }

  char t[1]; read(STDIN_FILENO, t, 1);
  close(connfd);
  close(listenfd);

  return 0;
}
