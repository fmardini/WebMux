#include "flash_policy.h"

static void flash_policy_conn_cb(EV_P_ ev_io *w, int revents) {
  if (revents & EV_ERROR) { ev_break(EV_A_ EVBREAK_ALL); return; }
  while (1) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(struct sockaddr_in);
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

static void flash_policy_write_cb(EV_P_ ev_io *w, int revents) {
  if (revents & EV_ERROR) { goto scamper; }
  ssize_t n;
  n = write(*(int *)w->data, FLASH_POLICY_STRING, strlen(FLASH_POLICY_STRING));
  if (n != strlen(FLASH_POLICY_STRING)) { perror("COULDNT WRITE FLASH POLICY"); }
 scamper:
  // ALWAYS close the connection after writing regardless of whether the it succeeded
  if (close(*(int *)w->data) == -1) {
    perror("ERROR WRITING TO CLIENT");
  }
  free(w->data);
  ev_io_stop(EV_A_ w);
  free(w);
}

void serve_flash_policy(EV_P) {
  int listen_fd = create_listening_socket(843);
  ev_io *w = malloc(sizeof(ev_io));
  w->data = malloc(sizeof(listen_fd));
  memcpy(w->data, &listen_fd, sizeof(listen_fd));
  ev_io_init(w, flash_policy_conn_cb, listen_fd, EV_READ);
  ev_io_start(EV_A_ w);
}

