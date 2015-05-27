#ifndef __TEST_CLIENT_H__
#define __TEST_CLIENT_H__

#include <czmq.h>

extern zsock_t * client;

void client_event();
void client_init();
void client_del();
#endif
