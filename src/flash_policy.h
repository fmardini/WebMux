#ifndef _FLASH_POLICY_H
#define _FLASH_POLICY_H

#include "common.h"
#include "net_helpers.h"

#define FLASH_POLICY_STRING "<cross-domain-policy><allow-access-from domain='*' to-ports='*' secure='false' /></cross-domain-policy>\0"

static void flash_policy_write_cb(EV_P_ ev_io *w, int revents);
static void flash_policy_conn_cb(EV_P_ ev_io *w, int revents);
void serve_flash_policy(EV_P);

#endif
