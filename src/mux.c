#include <ctype.h>
#include <errno.h>

#include "websocket.h"
#include "callbacks.h"

#define TRY_OR_ERR(STATUS, CALL, ERR)           \
  do {                                          \
    STATUS = (CALL)                             \
    if (STATUS < 0) { (ERR) }                   \
  } while (0)

unsigned int connHash(const void *key) {
  return dictGenHashFunction(key, strlen(key));
}

int connComp(void *privdata, const void *key1, const void *key2) {
  int l1 = strlen(key1), l2 = strlen(key2);
  return l1 == l2 && memcmp(key1, key2, l1) == 0;
}

void connKeyDest(void *_, void *key) {
  free(key);
}
void connValDest(void *_, void *val) {
  // freeing the connection is handled elsewhere
  free(((muxConn *)val)->outBuf);
  free(val);
}

// table from connfd to muxConn
static dictType connectionDict = {
  /* hashFunction  */ connHash,
  /* keyDup        */ NULL,
  /* valDup        */ NULL,
  /* keyCompare    */ connComp,
  /* keyDestructor */ connKeyDest,
  /* valDestructor */ NULL // connValDest
};

dict * active_connections;

void check_error(int res, char *msg) {
  if (res >= 0) return;
  fprintf(stderr, "Error (%s): %s\n", msg, strerror(errno));
}

http_parser_settings settings;

// JUST FOR TESTING
static void random_events(EV_P_ ev_timer *w, int revents) {
  dictEntry *de;
  int *cnt = w->data;
  dictIterator *iter = dictGetIterator(active_connections);
  while ((de = dictNext(iter)) != NULL) {
    unsigned char buf[32];
    snprintf(buf, 32, "HOLA AMIGOS %d", *cnt);
    write_to_client(EV_A_ de->val, 1, buf, strlen(buf));
    (*cnt)++;
  }
  dictReleaseIterator(iter);
}

int main(int argc, char **argv) {
  active_connections = dictCreate(&connectionDict, NULL);

  int listenfd, optval = 1;
  struct sockaddr_in servaddr;
  ev_default_loop(EVFLAG_SIGNALFD);

  memset(&servaddr, 0, sizeof(servaddr));
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  check_error(listenfd, "listen");
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(4000);
  check_error(bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)), "bind");
  check_error(listen(listenfd, 511), "listen");
  set_nonblock(listenfd);

  settings.on_header_field = on_header_field;
  settings.on_header_value = on_header_value;
  settings.on_message_complete = on_complete;
  settings.on_headers_complete = on_headers_complete;
  settings.on_path = on_path;

  ev_io new_connection_watcher;
  new_connection_watcher.data = &listenfd;
  ev_io_init(&new_connection_watcher, listening_socket_cb, listenfd, EV_READ);
  ev_io_start(EV_DEFAULT_ &new_connection_watcher);

  ev_timer gen_events;
  int timer_counter = 0;
  gen_events.data = &timer_counter;
  ev_timer_init(&gen_events, random_events, 0, 5);
  ev_timer_start(EV_DEFAULT_ &gen_events);

  ev_signal sigpipe_watcher;
  ev_signal_init(&sigpipe_watcher, sigpipe_cb, SIGPIPE);
  ev_signal_start(EV_DEFAULT_ &sigpipe_watcher);
  ev_signal sigint_watcher;
  ev_signal_init(&sigint_watcher, shutdown_server, SIGINT);
  ev_signal_start(EV_DEFAULT_ &sigint_watcher);

  ev_run(EV_DEFAULT_ 0);

  close(listenfd);
  return 0;
}
