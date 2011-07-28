#ifndef _NET_HELPERS_H
#define _NET_HELPERS_H

#include "common.h"

int set_nonblock(int fd);
int create_listening_socket(int port);
// inplace URI unescape
// returns len of string or -1 on error
int uri_unescape(char *raw, int raw_len);

#endif
