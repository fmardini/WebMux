#include "flash_protocol.h"

void fp_initialize(transport *xprt) {
  IGNORE_VAR(xprt);
  return;
}

void fp_conn_cb(muxConn *mc) {
  fp_transport_data *data = calloc(1, sizeof(fp_transport_data));
  mc->transport_data = data;
}

void fp_read_cb(muxConn *mc, ssize_t recved) {
  fp_transport_data *data = mc->transport_data;
  char *p = mc->in_buf + mc->in_buf_contents;
  for (int i = 0; i < recved; i++, p++) {
    if (*p == '\n') { // TODO: line terminated by \r\n
      mc->xprt->xprt_recv_msg(mc, mc->in_buf + data->cur_line_start, p - 1 - (mc->in_buf + data->cur_line_start));
      data->cur_line_start = (p - mc->in_buf) + 1; // next byte is the start of the new line
    }
  }
  if (*(p - 1) == '\n') {
    mc->in_buf_contents = 0;
    data->cur_line_start = 0;
  } else {
    mc->in_buf_contents += recved;
  }
}

void fp_recv_msg(muxConn *mc, char *msg, int msg_len) {
  write(STDOUT_FILENO, msg, msg_len);
}

void fp_write_out(muxConn *mc, char *msg, int msg_len) {
  fp_transport_data *data = mc->transport_data;
  // should not attempt to write to the client before it sends the handshake
  int needed = mc->outBufOffset + mc->outBufToWrite + msg_len + 2; // \r\n
  // grow output buffer if needed
  if (mc->outBufLen < needed) {
    mc->outBufLen = needed * 1.2;
    mc->outBuf = realloc(mc->outBuf, mc->outBufLen);
  }
  int p = mc->outBufOffset + mc->outBufToWrite;
  memcpy(mc->outBuf + p, msg, msg_len);
  mc->outBufToWrite += msg_len + 2;
  if (!ev_is_active(mc->watcher)) {
# if EV_MULTIPLICITY
    ev_io_start(mc->loop, mc->watcher);
# else
    ev_io_start(mc->watcher);
# endif
  }
}

void fp_free_data(muxConn *mc) {
  free(mc->transport_data);
}

static void connectCallback(const redisAsyncContext *c, int status) {
  if (status != REDIS_OK) {
    printf("Error: %s\n", c->errstr);
    perror("FNAT");
    exit(1);
  }
}

void uq_redis_connect(EV_P) {
  pubContext = redisAsyncConnect("127.0.0.1", 6379);
  if (pubContext->err) {
    fprintf(stderr, "Redis Error %s\n", pubContext->errstr);
    exit(1);
  }
  redisLibevAttach(EV_A_ pubContext);
  redisAsyncSetConnectCallback(pubContext, connectCallback);
}

