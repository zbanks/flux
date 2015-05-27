#include "broker/broker.h"
#include "core/err.h"
#include "server/server.h"
#include "client/client.h"
#include <czmq.h>

zsock_t * client;

int client_event(){
    int rc;
    char * string;
    if((rc = zsock_recv(client, "s", &string))) return rc;
    if(!string){
        printf("Error reading string from msg\n");
        return -1;
    }

    printf("Client recieved: '%s'\n", string);

    free(string);
    return 0;
}

void client_init(){
    printf("Client starting on localhost:" BROKER_PORT "...\n");

    client = zsock_new_req("tcp://localhost:" BROKER_PORT);
    if(!client) FAIL("Unable to open ZMQ REQ socket\n");
}

void client_del(){
    zsock_destroy(&client);
    if(client) FAIL("Error destroying client\n");

    printf("Client closed.\n");
}

