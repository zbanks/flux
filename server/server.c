#include "server/server.h"
#include "core/err.h"
#include <czmq.h>

zsock_t * server;

void server_event(){
    char * string;
    if(zsock_recv(server, "s", &string)) return;
    if(!string) FAIL("Server recieved invalid string\n");

    printf("Server recieved: '%s'\n", string);

    zsock_send(server, "s", string);

    free(string);
}

void server_init(){
    printf("Server starting on " SERVER_ENDPOINT "...\n");

    server = zsock_new_rep(">" SERVER_ENDPOINT);
    if(!server) FAIL("Unable to open ZMQ REP socket\n");
}

void server_del(){
    zsock_destroy(&server);
    if(server) FAIL("Error destroying reader\n");

    printf("Server closed.\n");
}
