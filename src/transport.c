#include "transport.h"

int setup_transport(EV_P_ transport *xprt) {
  int accept_fd            = create_listening_socket(xprt->port);
  accept_watcher_data *awd = calloc(1, sizeof(accept_watcher_data));
  awd->accept_fd           = accept_fd;
  awd->xprt                = xprt;
  awd->uniq_id             = 0;

  ev_io *new_connection_watcher = malloc(sizeof(ev_io));
  new_connection_watcher->data = awd;
  ev_io_init(new_connection_watcher, conn_accept_cb, accept_fd, EV_READ);
  xprt->xprt_initialize(xprt);
  ev_io_start(EV_DEFAULT_ new_connection_watcher);
}
