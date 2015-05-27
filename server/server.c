#include "server/server.h"
#include "core/err.h"
#include <czmq.h>
#include "lib/mdwrkapi.h"

zsock_t * server;

int server_event(){
    char * string;
    int rc;
    if((rc = zsock_recv(server, "s", &string))) return rc;
    if(!string){
        printf("Server recieved invalid string\n");
        return -1;
    }

    printf("Server recieved: '%s'\n", string);

    rc = zsock_send(server, "s", string);
    free(string);

    return rc;
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
