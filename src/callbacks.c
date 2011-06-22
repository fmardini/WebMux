#include "callbacks.h"

extern http_parser_settings settings;
extern dict *active_connections;

void client_write_cb(EV_P_ ev_io *w, int revents) {
  muxConn *mc = w->data;
  if (revents & EV_ERROR) { disconnectAndClean(EV_A_ mc->read_watcher); return; }
  if (mc->outBufToWrite > 0) {
    ssize_t n;
    n = write(mc->connfd, mc->outBuf + mc->outBufOffset, mc->outBufToWrite);
    if (n < 0) {
      disconnectAndClean(EV_A_ mc->read_watcher);
      perror("ERROR WRITING TO CLIENT");
      return;
    }
    mc->outBufToWrite -= n; mc->outBufOffset += n;
  }
  if (mc->outBufToWrite == 0) {
    mc->outBufOffset = 0;
    ev_io_stop(EV_A_ w);
  }
}

void client_read_cb(EV_P_ ev_io *w, int revents) {
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

  if (!conn->handshakeDone) {
    // no need to update in_buf_contents, once parsed data can be discarded
    ssize_t len_parsed;
    http_parser *parser = conn->parser;
    len_parsed = http_parser_execute(parser, &settings, conn->in_buf, recved);

    if (parser->upgrade) {
      // the +1 is needed since in this case conn->in_buf[len_parsed] is the \n, BUG??
      if (recved - len_parsed - 1 == 8) {
        memcpy(conn->body, conn->in_buf + len_parsed + 1, recved - len_parsed - 1);
      } else {
        fprintf(stderr, "INVALID WEBSOKCET HANDSHAKE BODY LENGTH\n");
        disconnectAndClean(EV_A_ w);
        return;
      }
    } else if (len_parsed != recved) {
      fprintf(stderr, "WEBSOCKET HANDSHAKE PARSE ERROR\n");
      disconnectAndClean(EV_A_ w);
      return;
    }
    if (handshake_connection(conn) != 0) { // TODO: check IF ALL IS VALID???
      fprintf(stderr, "INVALID CLIENT HANDSHAKE\n");
      disconnectAndClean(EV_A_ w);
      return;
    }
  } else { // TODO: implement websockets framing
    int i;
    for (i = 0; i < recved; i++, p++) {
      if (*(unsigned char *)p == 0x00) {
        conn->cur_frame_start = conn->in_buf_contents;
        conn->websocket_in_frame = 1;
      } else if (*(unsigned char *)p == 0xFF) { // frame from cur_frame_start to p
        // TODO: emit message received
        int n = write(STDOUT_FILENO, conn->in_buf + conn->cur_frame_start + 1, p - 1 - (conn->in_buf + conn->cur_frame_start));
        if (n < 0) { fprintf(stderr, "write failed\n"); }
        conn->websocket_in_frame = 0;
      }
    }
    if (!conn->websocket_in_frame) {
      conn->in_buf_contents = 0;
    } else {
      conn->in_buf_contents += recved;
    }
  }
}

void listening_socket_cb(EV_P_ ev_io *w, int revents) {
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
    http_parser *parser = (http_parser *)malloc(sizeof(http_parser));
    http_parser_init(parser, HTTP_REQUEST);
    parser->data = conn;
    conn->parser = parser;
    conn->connfd = connfd;
    conn->outBufLen = 1024;
    conn->outBuf = malloc(conn->outBufLen);
    conn->outBufToWrite = conn->outBufOffset = 0;

    conn->loop = loop;
    ev_io *client_connection_watcher = malloc(sizeof(ev_io));
    conn->read_watcher = client_connection_watcher;
    client_connection_watcher->data = conn;
    ev_io_init(client_connection_watcher, client_read_cb, connfd, EV_READ);
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

int write_to_client(EV_P_ muxConn *mc, int add_frame, unsigned char *msg, size_t msg_len) {
  // should not attempt to write to the client before it sends the handshake
  assert(mc->handshakeDone);
  int needed = mc->outBufOffset + mc->outBufToWrite + msg_len;
  // grow output buffer if needed
  if (mc->outBufLen < needed)
    mc->outBuf = realloc(mc->outBuf, needed * 1.2);
  int p = mc->outBufOffset + mc->outBufToWrite;
  if (add_frame) mc->outBuf[p] = '\x00';
  memcpy(mc->outBuf + p + (add_frame ? 1 : 0), msg, msg_len);
  if (add_frame) mc->outBuf[p + 1 + msg_len] = '\xFF';
  mc->outBufToWrite += msg_len + (add_frame ? 2 : 0);
  if (!ev_is_active(mc->watcher)) {
    ev_io_start(EV_A_ mc->watcher);
  }
  return 0;
}

// takes the read watcher
void disconnectAndClean(EV_P_ ev_io *w) {
  muxConn *mc = w->data;
  dictDelete(active_connections, mc->connKey);
  free_mux_conn(mc);
  ev_io_stop(EV_A_ w);
}

// ignore sigpipe
void sigpipe_cb(EV_P_ ev_signal *w, int revents) { IGNORE_VAR(EV_A); IGNORE_VAR(w); IGNORE_VAR(revents); }

void shutdown_server(EV_P_ ev_signal *w, int revents) {
  IGNORE_VAR(w); IGNORE_VAR(revents);
  IGNORE_VAR(dictGenHashFunction); IGNORE_VAR(dictCreate); IGNORE_VAR(dictReplace); IGNORE_VAR(dictRelease);
  dictEntry *de;
  dictIterator *iter = dictGetIterator(active_connections);
  while ((de = dictNext(iter)) != NULL) {
    muxConn * mc = de->val;
    // stop any pending writes
    ev_io_stop(EV_A_ mc->watcher);
    disconnectAndClean(EV_A_ mc->read_watcher);
  }
  dictReleaseIterator(iter);
  ev_break(EV_A_ EVBREAK_ALL);
}


