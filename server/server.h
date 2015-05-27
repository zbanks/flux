#ifndef __SERVER_SERVER_H__
#define __SERVER_SERVER_H__

#include <czmq.h>

#define SERVER_ENDPOINT "tcp://127.0.0.1:5568"

extern zsock_t * server;

void server_event();
void server_init();
void server_del();

#endif
