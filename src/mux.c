#include "common.h"

#include "callbacks.h"
#include "flash_policy.h"
#include "flash_protocol.h"

// table from connfd to muxConn
unsigned int intKeyHash(const void *key) {
  int t = (int)key;
  return dictGenHashFunction(&t, sizeof(t));
}

int intKeyCompare(void *privdata, const void *key1, const void *key2) {
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

dict * active_connections;
http_parser_settings settings;

// JUST FOR TESTING
static void random_events(EV_P_ ev_timer *w, int revents) {
  return;
  IGNORE_VAR(revents);
  dictEntry *de;
  int *cnt = w->data;
  dictIterator *iter = dictGetIterator(active_connections);
  while ((de = dictNext(iter)) != NULL) {
    char buf[32];
    snprintf(buf, 32, "HOLA AMIGOS %d", *cnt);
    write_to_client(EV_A_ de->val, 1, (unsigned char *)buf, strlen(buf));
    (*cnt)++;
  }
  dictReleaseIterator(iter);
}

extern dict *active_connections;

int main(int argc, char **argv) {
  active_connections = dictCreate(&connectionDict, NULL);

  int listenfd, optval = 1, res;
  struct sockaddr_in servaddr;
  ev_default_loop(EVFLAG_SIGNALFD);

  memset(&servaddr, 0, sizeof(servaddr));
  TRY_OR_EXIT(listenfd, socket(AF_INET, SOCK_STREAM, 0), "socket");
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(4000);
  TRY_OR_EXIT(res, bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)), "bind");
  TRY_OR_EXIT(res, listen(listenfd, 511), "listen");
  set_nonblock(listenfd);

  settings.on_header_field = on_header_field;
  settings.on_header_value = on_header_value;
  settings.on_message_complete = on_complete;
  settings.on_headers_complete = on_headers_complete;
  settings.on_path = on_path;

  int flash_policy_fd = flash_policy_accept_socket();
  ev_io policy_conn_watcher;
  policy_conn_watcher.data = malloc(sizeof(flash_policy_fd));
  memcpy(policy_conn_watcher.data, &flash_policy_fd, sizeof(flash_policy_fd));
  ev_io_init(&policy_conn_watcher, flash_policy_conn_cb, flash_policy_fd, EV_READ);
  ev_io_start(EV_DEFAULT_ &policy_conn_watcher);

  int flash_protocol_fd = flash_protocol_socket_fd();
  ev_io flash_protocol_watcher;
  accept_watcher_data *awd = calloc(1, sizeof(accept_watcher_data));
  awd->accept_fd = flash_protocol_fd;
  awd->uniq_id = 0;
  flash_protocol_watcher.data = awd;
  ev_io_init(&flash_protocol_watcher, flash_protocol_conn_cb, flash_protocol_fd, EV_READ);
  ev_io_start(EV_DEFAULT_ &flash_protocol_watcher);

  ev_io new_connection_watcher;
  new_connection_watcher.data = &listenfd;
  ev_io_init(&new_connection_watcher, listening_socket_cb, listenfd, EV_READ);
  ev_io_start(EV_DEFAULT_ &new_connection_watcher);

  ev_signal sigpipe_watcher;
  ev_signal_init(&sigpipe_watcher, sigpipe_cb, SIGPIPE);
  ev_signal_start(EV_DEFAULT_ &sigpipe_watcher);
  ev_signal sigint_watcher;
  ev_signal_init(&sigint_watcher, shutdown_server, SIGINT);
  ev_signal_start(EV_DEFAULT_ &sigint_watcher);

  uq_redis_connect(EV_DEFAULT);

  ev_run(EV_DEFAULT_ 0);

  close(listenfd);
  dictRelease(active_connections);
  IGNORE_VAR(dictReplace); IGNORE_VAR(dictDelete);

  return 0;
}
