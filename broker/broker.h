#ifndef __BROKER_BROKER_H__
#define __BROKER_BROKER_H__

#include <czmq.h>

#define BROKER_ENDPOINT "tcp://127.0.0.1:5560"

extern zsock_t * broker_front;
extern zsock_t * broker_back;

void broker_front_event();
void broker_back_event();
void broker_init();
void broker_del();
#endif
