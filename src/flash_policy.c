#include "flash_policy.h"

void flash_policy_conn_cb(EV_P_ ev_io *w, int revents) {
  if (revents & EV_ERROR) { ev_break(EV_A_ EVBREAK_ALL); return; }
  while (1) {
    struct sockaddr_in addr;
    socklen_t len;
    int connfd = accept(*(int *)w->data, (struct sockaddr *)&addr, &len);
    if (connfd == -1) {
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNABORTED && errno != EPROTO) {
        // HANDLE ERROR, e.g. EMFILE
      }
      break;
    }
    set_nonblock(connfd);

    ev_io *policy_write_watcher = malloc(sizeof(ev_io));
    policy_write_watcher->data = malloc(sizeof(connfd));
    memcpy(policy_write_watcher->data, &connfd, sizeof(connfd));
    ev_io_init(policy_write_watcher, flash_policy_write_cb, connfd, EV_WRITE);
    ev_io_start(EV_A_ policy_write_watcher);
  }
}

void flash_policy_write_cb(EV_P_ ev_io *w, int revents) {
  if (revents & EV_ERROR) { goto scamper; }
  ssize_t n;
  n = write(*(int *)w->data, FLASH_POLICY_STRING, strlen(FLASH_POLICY_STRING));
  if (n != strlen(FLASH_POLICY_STRING)) { fprintf(stderr, "%s\n", strerror(errno)); }
 scamper:
  // ALWAYS close the connection after write regardless of whether the it succeeded
  if (close(*(int *)w->data) == -1) {
    perror("ERROR WRITING TO CLIENT");
  }
  free(w->data);
  ev_io_stop(EV_A_ w);
  free(w);
}

int flash_policy_accept_socket(void) {
  int listenfd, optval = 1, res;
  struct sockaddr_in servaddr;

  memset(&servaddr, 0, sizeof(servaddr));
  TRY_OR_EXIT(listenfd, socket(AF_INET, SOCK_STREAM, 0), "socket");
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(843);
  TRY_OR_EXIT(res, bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)), "bind");
  TRY_OR_EXIT(res, listen(listenfd, 511), "listen");
  set_nonblock(listenfd);
  return listenfd;
}
