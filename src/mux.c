#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
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

void check_error(int res, char *msg) {
  if (res >= 0) return;
  fprintf(stderr, "Error (%s): %s\n", msg, strerror(errno));
}

int done_with_req;
int on_complete(http_parser *_) {
  done_with_req = 1;
  return 0;
}

int on_headers_complete(http_parser *parser) {
  puts("headers complete");
  return 0;
}

int on_body(http_parser *parser, const char *at, size_t len) {
  write(STDOUT_FILENO, at, len);
  return 0;
}

int count_spaces(char *s) {
  int i, count = 0;
  for (i = 0; s[i]; i++)
    if (s[i] == ' ') count++;
  return count;
}

int digits_from_str(char *s) {
  int i, j = 0;
  char *res = (char *)malloc(sizeof(char) * (strlen(s) + 1));
  for (i = 0; s[i]; i++)
    if (s[i] >= '0' && s[i] <= '9') res[j++] = s[i];
  res[j] = '\0';
  i = atoi(res);
  free(res);
  return i;
}


unsigned char *compute_handshake(char *f1, char *f2, char *last8) {
  unsigned int k1 = digits_from_str(f1);
  unsigned int k2 = digits_from_str(f2);
  int s1 = count_spaces(f1);
  int s2 = count_spaces(f2);

  if (s1 == 0 || s2 == 0) perror("invalid handshake");
  unsigned int v1 = htonl(k1 / s1), v2 = htonl(k2 / s2);
  unsigned char kk[16];
  memcpy(kk, &v1, 4);
  memcpy(kk + 4, &v2, 4);
  memcpy(kk + 8, last8, 8);
  unsigned char *out = (unsigned char *)calloc(1, 17 * sizeof(char));
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, kk, 16);
  MD5_Final(out, &ctx);
  return out;
}

// static char headers_fields[] = {
  // "Sec-WebSocket-Protocol", // echoed by server to indicate it supports the protocol
  // "Host",                   // included by server in Sec-WebSocket-Location
  // "Origin",                 // server includes a Sec-WebSocket-Origin field with that origin
  // "Sec-WebSocket-Key1",
  // "Sec-WebSocket-Key2"

  // RESPONSE
  // HTTP/1.1 101 WebSocket Protocol Handshake
  // Upgrade: WebSocket
  // Connection: Upgrade
  // Sec-WebSocket-Origin: http://example.com
  // Sec-WebSocket-Location: ws://example.com/
// };

#define CURRENT_LINE (&headers[num_headers])

struct header {
  char *field;
  size_t field_len;
  char *value;
  size_t value_len;
} headers[100];
static int num_headers = 0;
static int last_was_value = 0;

int on_header_field (http_parser *_, const char *at, size_t len) {
  if (last_was_value) {
    num_headers++;

    CURRENT_LINE->value = NULL;
    CURRENT_LINE->value_len = 0;

    CURRENT_LINE->field_len = len;
    CURRENT_LINE->field = malloc(len+1);
    strncpy(CURRENT_LINE->field, at, len);

  } else {
    assert(CURRENT_LINE->value == NULL);
    assert(CURRENT_LINE->value_len == 0);

    CURRENT_LINE->field_len += len;
    CURRENT_LINE->field = realloc(CURRENT_LINE->field, CURRENT_LINE->field_len + 1);
    strncat(CURRENT_LINE->field, at, len);
  }

  CURRENT_LINE->field[CURRENT_LINE->field_len] = '\0';
  last_was_value = 0;
  return 0;
}

int on_header_value (http_parser *_, const char *at, size_t len) {
  if (!last_was_value) {
    CURRENT_LINE->value_len = len;
    CURRENT_LINE->value = malloc(len+1);
    strncpy(CURRENT_LINE->value, at, len);

  } else {
    CURRENT_LINE->value_len += len;
    CURRENT_LINE->value = realloc(CURRENT_LINE->value, CURRENT_LINE->value_len + 1);
    strncat(CURRENT_LINE->value, at, len);
  }

  CURRENT_LINE->value[CURRENT_LINE->value_len] = '\0';
  last_was_value = 1;
  return 0;
}

static char *req_path;
int on_path(http_parser *_, const char *at, size_t len) {
  req_path = (char *)malloc(sizeof(char) * len);
  memcpy(req_path, at, len);
  return 0;
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
  settings.on_header_field = on_header_field;
  settings.on_header_value = on_header_value;
  settings.on_message_complete = on_complete;
  settings.on_body             = on_body;
  settings.on_path             = on_path;
  settings.on_headers_complete = on_headers_complete;
  http_parser *parser = (http_parser *)malloc(sizeof(http_parser));
  http_parser_init(parser, HTTP_REQUEST);

  char buf[1024 * 4];
  char body[8];
  done_with_req = 0;
  ssize_t recved;
  ssize_t len_parsed;

  while (!done_with_req) {
    recved = recv(connfd, buf, 4 * 1024, 0);
    len_parsed = http_parser_execute(parser, &settings, buf, recved);

    if (parser->upgrade) {
      memcpy(body, buf + len_parsed + 1, recved - len_parsed - 1);
    } else if (len_parsed != recved)
      fprintf(stderr, "PARSE ERROR\n");
  }
  done_with_req = 0;

  int i;
  char *keys[2], *p, *origin, *host;
  for (i = 0; i <= num_headers; i++) {
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
    unsigned char *hand_shake = compute_handshake(keys[0], keys[1], body);
    char loc[256];
    snprintf(loc, 256, "%s%s", host, req_path);
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
