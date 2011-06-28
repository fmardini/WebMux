#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

#include <openssl/md5.h>
#include "common.h"

// Forward declarations
int write_to_client(EV_P_ muxConn *mc, int add_frame, unsigned char *msg, size_t msg_len);

int process_key(char *k, unsigned int *res);
int compute_checksum(char *f1, char *f2, char *last8, unsigned char *out);
int server_handshake(unsigned char *md5, char *origin, char *loc, char *protocol, char *resp, int resp_len);
void free_mux_conn(muxConn *conn);
int handshake_connection(muxConn *conn);

#define CURRENT_LINE(_mc) (&(_mc)->hs.list[(_mc)->hs.num_headers])

void process_last_header(muxConn *conn);
int on_header_field(http_parser *parser, const char *at, size_t len);
int on_header_value(http_parser *parser, const char *at, size_t len);
int on_headers_complete(http_parser *parser);
int on_complete(http_parser *parser);
int on_path(http_parser *parser, const char *at, size_t len);

#endif
