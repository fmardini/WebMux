#ifndef _WEB_MUX_COMMON_H
#define _WEB_MUX_COMMON_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <ev.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>


#define IGNORE_VAR(x) ((void) (x))

#define TRY_OR_EXIT(STATUS, CALL, MSG)       \
  do {                                       \
    STATUS = (CALL);                         \
    if (STATUS < 0) {                        \
      perror((MSG));                         \
      exit(1);                               \
    }                                        \
  } while (0)


#define MAX_HEADERS 32

typedef struct header {
  char   *field;
  size_t field_len;
  char   *value;
  size_t value_len;
} header;

typedef struct {
  header list[MAX_HEADERS];
  int last_was_value;
  int num_headers;
} headers;

#define LOG(m1,m2) fprintf(stdout, "%s: %s\n", m1, m2);

#endif
