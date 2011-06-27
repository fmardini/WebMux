#ifndef _WEB_MUX_COMMON_H
#define _WEB_MUX_COMMON_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <ev.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#define TRY_OR_EXIT(STATUS, CALL, MSG)       \
  do {                                       \
    STATUS = (CALL);                         \
    if (STATUS < 0) {                        \
      perror((MSG));                         \
      exit(1);                               \
    }                                        \
  } while (0)

#endif
