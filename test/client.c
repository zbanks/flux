#include "core/err.h"
#include "server/server.h"
#include "test/client.h"
#include <czmq.h>

zsock_t * client;

void client_event(){
    char * string;
    if(zsock_recv(client, "s", &string)) return;
    if(!string) FAIL("Error reading string from msg\n");

    printf("Client recieved: '%s'\n", string);

    free(string);
}

void client_init(){
    printf("Client starting on " SERVER_ENDPOINT "...\n");

    client = zsock_new_req("@" SERVER_ENDPOINT);
    if(!client) FAIL("Unable to open ZMQ REQ socket\n");
}

void client_del(){
    zsock_destroy(&client);
    if(client) FAIL("Error destroying client\n");

    printf("Client closed.\n");
}

