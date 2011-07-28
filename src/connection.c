#include "connection.h"

// ignore sigpipe
static void sigpipe_cb(EV_P_ ev_signal *w, int revents) {
  IGNORE_VAR(EV_A); IGNORE_VAR(w); IGNORE_VAR(revents);
  return;
}

static void shutdown_server(EV_P_ ev_signal *w, int revents) {
  IGNORE_VAR(w); IGNORE_VAR(revents);
  IGNORE_VAR(dictGenHashFunction); IGNORE_VAR(dictCreate); IGNORE_VAR(dictReplace); IGNORE_VAR(dictRelease);
  dictEntry *de;
  dictIterator *iter = dictGetIterator(active_connections);
  while ((de = dictNext(iter)) != NULL) {
    muxConn *mc = de->val;
    // stop any pending writes
    ev_io_stop(EV_A_ mc->watcher);
    disconnectAndClean(mc);
  }
  dictReleaseIterator(iter);
  ev_break(EV_A_ EVBREAK_ALL);
}

// table from connfd to muxConn
static unsigned int intKeyHash(const void *key) {
  int t = (int)key;
  return dictGenHashFunction(&t, sizeof(t));
}
static int intKeyCompare(void *privdata, const void *key1, const void *key2) {
  IGNORE_VAR(privdata);
  int k1 = (int)key1, k2 = (int)key2;
  return k1 == k2;
}
static dictType connectionDict = {
  /* hashFunction  */ intKeyHash,
  /* keyDup        */ NULL,
  /* valDup        */ NULL,
  /* keyCompare    */ intKeyCompare,
  /* keyDestructor */ NULL,
  /* valDestructor */ NULL
};
int initialize_connections(EV_P) {
  ev_signal *sigpipe_watcher = malloc(sizeof(ev_signal));
  ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
  ev_signal_start(EV_A_ sigpipe_watcher);
  ev_signal *sigint_watcher = malloc(sizeof(ev_signal));
  ev_signal_init(sigint_watcher, shutdown_server, SIGINT);
  ev_signal_start(EV_A_ sigint_watcher);

  active_connections = dictCreate(&connectionDict, NULL);
  return 0;
}
int finalize_connections() {
  dictRelease(active_connections);
  return 0;
}

void conn_accept_cb(EV_P_ ev_io *w, int revents) {
  if (revents & EV_ERROR) { perror("ERROR ACCEPTING CONNECTION"); ev_break(EV_A_ EVBREAK_ALL); return; }
  struct sockaddr_in addr;
  socklen_t len = sizeof(struct sockaddr_in);
  accept_watcher_data *awd = w->data;
  int connfd = accept(awd->accept_fd, (struct sockaddr *)&addr, &len);
  if (connfd == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNABORTED && errno != EPROTO) {
      // HANDLE ERROR, e.g. EMFILE
    }
    return;
  }
  set_nonblock(connfd);
  muxConn *conn       = calloc(1, sizeof(muxConn)); // ZEROED-OUT
  conn->conn_id       = ++(awd->uniq_id);
  conn->in_buf_len    = 1024 * 4;
  conn->in_buf        = malloc(conn->in_buf_len);
  conn->connfd        = connfd;
  conn->outBufLen     = 1024;
  conn->outBuf        = malloc(conn->outBufLen);
  conn->outBufToWrite = conn->outBufOffset = 0;
  conn->xprt          = awd->xprt;
#if EV_MULTIPLICITY
  conn->loop          = loop;
#endif

  // Read watcher
  ev_io *client_connection_watcher = malloc(sizeof(ev_io));
  conn->read_watcher = client_connection_watcher;
  client_connection_watcher->data = conn;
  ev_io_init(client_connection_watcher, conn_read_cb, connfd, EV_READ);
  ev_io_start(EV_A_ client_connection_watcher);
  // Write watcher
  ev_io *client_write_watcher = malloc(sizeof(ev_io));
  client_write_watcher->data  = conn;
  conn->watcher = client_write_watcher;
  // initialize write watcher but don't start it
  ev_io_init(client_write_watcher, conn_write_cb, connfd, EV_WRITE);

  awd->xprt->xprt_conn_cb(conn);

  dictAdd(active_connections, connfd, conn);
}

void conn_read_cb(EV_P_ ev_io *w, int revents) {
  muxConn *mc = w->data;
  if (revents & EV_ERROR) { disconnectAndClean(mc); return; }
  ssize_t recved;

  // if buffer full reallocate
  if (mc->in_buf_len == mc->in_buf_contents) {
    size_t new_len = mc->in_buf_len * 2;
    mc->in_buf = realloc(mc->in_buf, new_len);
    mc->in_buf_len = new_len;
  }
  recved = recv(mc->connfd, mc->in_buf + mc->in_buf_contents, mc->in_buf_len - mc->in_buf_contents, 0);
  if (recved <= 0) {
    if (recved < 0) perror("ERROR READING FROM CLIENT");
    disconnectAndClean(mc);
    return;
  }
  mc->xprt->xprt_read_cb(mc, recved);
}

void conn_write_cb(EV_P_ ev_io *w, int revents) {
  muxConn *mc = w->data;
  if (revents & EV_ERROR) { disconnectAndClean(mc); return; }
  if (mc->outBufToWrite > 0) {
    ssize_t n;
    n = write(mc->connfd, mc->outBuf + mc->outBufOffset, mc->outBufToWrite);
    if (n < 0) {
      disconnectAndClean(mc);
      perror("ERROR WRITING TO CLIENT");
      return;
    }
    mc->outBufToWrite -= n; mc->outBufOffset += n;
  }
  if (mc->outBufToWrite == 0) {
    // TODO: shrink buffer if too big
    mc->outBufOffset = 0;
    ev_io_stop(EV_A_ w);
  }
}

void disconnectAndClean(muxConn *mc) {
  if (DICT_ERR == dictDelete(active_connections, mc->connfd)) { fprintf(stderr, "(BUG) CONNECTION ALREADY REMOVED FROM HASH"); }
  close(mc->connfd);
  if (mc->user_id != NULL) free(mc->user_id);
  ev_io_stop(mc->loop, mc->watcher); ev_io_stop(mc->loop, mc->read_watcher);
  free(mc->watcher); free(mc->read_watcher);
  if (mc->updates_watcher != NULL) {
    ev_timer_stop(mc->loop, mc->updates_watcher); free(mc->updates_watcher);
  }
  free(mc->in_buf);
  free(mc->outBuf);

  mc->xprt->xprt_free_data(mc);
  free(mc);
}

