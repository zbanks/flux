#include "server/server.h"
#include "core/err.h"
#include <czmq.h>
#include <pthread.h>

static pthread_t server_thread;
static int server_running;

static void * server_run(void * args){
    UNUSED(args);

    printf("Server starting on " SERVER_ENDPOINT "...\n");

    zsock_t * reader = zsock_new_rep(">" SERVER_ENDPOINT);
    if(!reader) FAIL("Unable to open ZMQ REP socket\n");

    zpoller_t * poller = zpoller_new(reader);
    if(!poller) FAIL("Unable to set up reader poller\n");

    while(server_running){
        zsock_t * which = (zsock_t *) zpoller_wait(poller, SERVER_POLL_TIMEOUT_MSEC);
        if(!which) continue;
        if(which != reader) FAIL("Invalid socket returned from poller\n");

        char * string;
        if(zsock_recv(reader, "s", &string)) break;
        if(!string) FAIL("Error reading string from msg\n");

        printf("Recieved: '%s'\n", string);

        if(zsock_send(reader, "s", string)) break;

        free(string);
    }
    server_running = 0;

    zsock_destroy(&reader);
    if(reader) FAIL("Error destroying reader\n");

    printf("Server closed.\n");
    return 0;
}

void server_init(){
    server_running = 1;
    if(pthread_create(&server_thread, 0, server_run, 0))
        FAIL("Error starting server thread\n");
}

void server_del(){
    server_running = 0;
    if(pthread_join(server_thread, 0))
        FAIL("Error waiting for server thread to join.\n");
}
