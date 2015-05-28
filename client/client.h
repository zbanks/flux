#ifndef __CLIENT_CLIENT_H__
#define __CLIENT_CLIENT_H__

#include "lib/mdcliapi.h"
#include <czmq.h>

extern mdcli_t * client;

int client_event();
void client_init(int verbose);
void client_del();
#endif
