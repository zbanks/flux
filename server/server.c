#include "server/server.h"
#include "core/err.h"
#include <czmq.h>
#include "lib/mdwrkapi.h"

mdwrk_t * server;

int server_event(){
    static zmsg_t * reply = NULL;

    zmsg_t * request =  mdwrk_recv(server, &reply);
    if(request == NULL) return -1;

    reply = request;

    return 0;
}

void server_init(){
    printf("Server starting on " SERVER_ENDPOINT "...\n");
    server = mdwrk_new("tcp://localhost:5555", "lux", 1);
    if(!server) FAIL("Unable to open ZMQ worker socket\n");
}

void server_del(){
    mdwrk_destroy(&server);
    if(server) FAIL("Error destroying worker\n");

    printf("Server closed.\n");
}
