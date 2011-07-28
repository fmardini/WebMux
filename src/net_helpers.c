#include "net_helpers.h"

int set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    return -1;
  return 0;
}

int create_listening_socket(int port) {
  int listenfd, optval = 1, res;
  struct sockaddr_in servaddr;

  memset(&servaddr, 0, sizeof(servaddr));
  TRY_OR_EXIT(listenfd, socket(AF_INET, SOCK_STREAM, 0), "socket");
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(port);
  TRY_OR_EXIT(res, bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)), "bind");
  TRY_OR_EXIT(res, listen(listenfd, 511), "listen");
  set_nonblock(listenfd);
  return listenfd;
}

// inplace URI unescape
// returns len of string or -1 on error
int uri_unescape(char *raw, int raw_len) {
  char *rp = raw, *wp = raw;
  int cnt = 0;
  while (raw_len--) {
    if (*rp == '%') {
      if (raw_len  < 2) { return -1; }
      char t[3] = { *(rp + 1), *(rp + 2), 0 };
      rp += 3;
      *wp++ = (char)strtol(t, NULL, 16);
    } else *wp++ = *rp++;
    cnt++;
  }
  return cnt;
}

