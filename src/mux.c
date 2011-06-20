#include <ctype.h>
#include <errno.h>
#include "hiredis.h"
#include "async.h"
#include "adapters/libev.h"

#include "websocket.h"
#include "callbacks.h"

#define TRY_OR_EXIT(STATUS, CALL, MSG)       \
  do {                                       \
    STATUS = (CALL);                         \
    if (STATUS < 0) {                        \
      perror((MSG));                         \
      exit(1);                               \
    }                                        \
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
http_parser_settings settings;

// JUST FOR TESTING
static void random_events(EV_P_ ev_timer *w, int revents) {
  return;
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

void get_updates(redisAsyncContext *ac, void *_r, void *priv) {
  // ((void) priv);
  redisReply *r = _r;
  assert(r->elements > 2);
  dictEntry *de;
  dictIterator *iter = dictGetIterator(active_connections);
  while ((de = dictNext(iter)) != NULL) {
    write_to_client(((redisLibevEvents *)ac->ev.data)->loop, de->val, 1, r->element[2]->str, r->element[2]->len);
  }
  dictReleaseIterator(iter);
}

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

  // Redis stuff
  redisAsyncContext *pubContext = redisAsyncConnect("127.0.0.1", 6379);
  if (pubContext->err) {
    fprintf(stderr, "Error %s\n", pubContext->errstr);
    exit(1);
  }
  redisLibevAttach(EV_DEFAULT_ pubContext);
  redisAsyncCommand(pubContext, get_updates, NULL, "SUBSCRIBE job_applications");

  ev_run(EV_DEFAULT_ 0);

  close(listenfd);
  return 0;
}
