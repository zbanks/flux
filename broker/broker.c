#include "broker/broker.h"
#include "core/err.h"
#include "server/server.h"
#include <czmq.h>

zsock_t * broker_front;
zsock_t * broker_back;

void broker_front_event(){

}

void broker_back_event(){

}

void broker_init(){
    printf("Broker starting on " BROKER_ENDPOINT "...\n");

    broker_front = zsock_new_router(">" BROKER_ENDPOINT);
    if(!broker_front) FAIL("Unable to open ZMQ ROUTER socket (broker_front).\n");

    broker_back = zsock_new_dealer("@" SERVER_ENDPOINT);
    if(!broker_back) FAIL("Unable to open ZMQ DEALER socket (broker_back).\n");
}

void broker_del(){
    zsock_destroy(&broker_front);
    if(broker_front) FAIL("Error destroying broker_front\n");

    zsock_destroy(&broker_back);
    if(broker_back) FAIL("Error destroying broker_back\n");

    printf("Broker closed.\n");
}
