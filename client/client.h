#ifndef __CLIENT_CLIENT_H__
#define __CLIENT_CLIENT_H__

#include <czmq.h>

extern zsock_t * client;

int client_event();
void client_init();
void client_del();
#endif
