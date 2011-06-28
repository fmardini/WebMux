#include "flash_protocol.h"

extern dict *active_connections;

void flash_protocol_read_cb(EV_P_ ev_io *w, int revents) {
  if (revents & EV_ERROR) { disconnectAndClean(EV_A_ w); return; }
  muxConn *conn = w->data;
  ssize_t recved;

  // if buffer full reallocate
  if (conn->in_buf_len == conn->in_buf_contents) {
    size_t new_len = conn->in_buf_len * 2;
    conn->in_buf = realloc(conn->in_buf, new_len);
    conn->in_buf_len = new_len;
  }
  char *p = conn->in_buf + conn->in_buf_contents;
  recved = recv(conn->connfd, p, conn->in_buf_len - conn->in_buf_contents, 0);
  if (recved <= 0) {
    if (recved < 0) perror("ERROR READING FROM CLIENT");
    disconnectAndClean(EV_A_ w);
    return;
  }

  int i;
  for (i = 0; i < recved; i++, p++) {
    if (*p == '\n') { // TODO: line terminated by \r\n
      int n = write(STDOUT_FILENO, conn->in_buf + conn->cur_frame_start, p - 2 - (conn->in_buf + conn->cur_frame_start)); // last two bytes are \r\n
      if (n < 0) { fprintf(stderr, "write failed\n"); }
      conn->cur_frame_start = (p - conn->in_buf) + 1; // next byte is new line
    }
  }
  if (*(p - 1) == '\n') {
    conn->in_buf_contents = 0;
    conn->cur_frame_start = 0;
  } else {
    conn->in_buf_contents += recved;
  }

}

void flash_protocol_conn_cb(EV_P_ ev_io *w, int revents) {
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

    muxConn *conn = (muxConn *)calloc(1, sizeof(muxConn)); // ZEROED-OUT
    conn->in_buf_len = 1024 * 4;
    conn->in_buf = malloc(conn->in_buf_len);
    conn->connfd = connfd;
    conn->outBufLen = 1024;
    conn->handshakeDone = 1;
    conn->outBuf = malloc(conn->outBufLen);
    conn->outBufToWrite = conn->outBufOffset = 0;

    conn->loop = loop;
    ev_io *client_connection_watcher = malloc(sizeof(ev_io));
    conn->read_watcher = client_connection_watcher;
    client_connection_watcher->data = conn;
    ev_io_init(client_connection_watcher, flash_protocol_read_cb, connfd, EV_READ);
    ev_io_start(EV_A_ client_connection_watcher);

    conn->connKey = malloc(16);
    sprintf(conn->connKey, "%d", connfd);
    dictAdd(active_connections, conn->connKey, conn);
    ev_io *client_write_watcher = malloc(sizeof(ev_io));
    client_write_watcher->data  = conn;
    conn->watcher = client_write_watcher;
    // initialize write watcher but don't start it
    ev_io_init(client_write_watcher, client_write_cb, connfd, EV_WRITE);
  }
}

int flash_protocol_socket_fd(void) {
  int listenfd, optval = 1, res;
  struct sockaddr_in servaddr;

  memset(&servaddr, 0, sizeof(servaddr));
  TRY_OR_EXIT(listenfd, socket(AF_INET, SOCK_STREAM, 0), "socket");
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(4167);
  TRY_OR_EXIT(res, bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)), "bind");
  TRY_OR_EXIT(res, listen(listenfd, 511), "listen");
  set_nonblock(listenfd);
  return listenfd;
}
