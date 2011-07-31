#include "common.h"

#include "flash_policy.h"
#include "flash_protocol.h"
#include "websocket.h"
#include "polling.h"

transport ws_transport = {
  4000,
  ws_initialize,
  ws_conn_cb,
  ws_read_cb,
  ws_recv_msg,
  ws_write_out,
  ws_free_data
};
transport fp_transport = {
  4167,
  fp_initialize,
  fp_conn_cb,
  fp_read_cb,
  fp_recv_msg,
  fp_write_out,
  fp_free_data
};
transport po_transport = {
  8080,
  po_initialize,
  po_conn_cb,
  po_read_cb,
  po_recv_msg,
  po_write_out,
  po_free_data
};

int main(int argc, char **argv) {
  ev_default_loop(EVFLAG_SIGNALFD);

  initialize_connections(EV_DEFAULT);
  setup_transport(EV_DEFAULT_ &ws_transport);
  setup_transport(EV_DEFAULT_ &fp_transport);
  setup_transport(EV_DEFAULT_ &po_transport);
  serve_flash_policy(EV_DEFAULT);

  uq_redis_connect(EV_DEFAULT);
  ev_run(EV_DEFAULT_ 0);

  finalize_connections();

  return 0;
}
