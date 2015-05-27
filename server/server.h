#ifndef __SERVER_SERVER_H__
#define __SERVER_SERVER_H__

#include <czmq.h>
#include "lib/mdwrkapi.h"

extern mdwrk_t * server;

int server_event();
void server_init();
void server_del();

#endif
