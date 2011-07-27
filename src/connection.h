#ifndef _WEB_MUX_CONNECTION_H
#define _WEB_MUX_CONNECTION_H

#include "common.h"
#include "transport.h"
#include "net_helpers.h"
#include "dict.c"

// Forward decls
struct _transport_;
struct _muxConn_;

typedef struct {
  int accept_fd;
  unsigned int uniq_id;
  struct _transport_ *xprt;
} accept_watcher_data;

struct _muxConn_ {
  // ATTRIBUTES
  unsigned int conn_id;
  int connfd;
  char *user_id;
  time_t timestamp;
  struct _transport_ *xprt;
  void *transport_data;
  // EVENT LOOP
#if EV_MULTIPLICITY
  struct ev_loop *loop;
#endif
  ev_io *watcher;
  ev_io *read_watcher;
  ev_timer *updates_watcher;
  // IO BUFFERS
  char *in_buf;
  size_t in_buf_len;
  size_t in_buf_contents;
  char *outBuf;
  int outBufLen;
  int outBufOffset;
  int outBufToWrite;
};

typedef struct _muxConn_ muxConn;

dict *active_connections;
int initialize_connections(EV_P);
int finalize_connections(void);

static void sigpipe_cb(EV_P_ ev_signal *w, int revents);
static void shutdown_server(EV_P_ ev_signal *w, int revents);

void conn_accept_cb(EV_P_ ev_io *w, int revents);
void conn_read_cb(EV_P_ ev_io *w, int revents);
void conn_write_cb(EV_P_ ev_io *w, int revents);
int create_listening_socket(int port);
void disconnectAndClean(muxConn *mc);

#endif
