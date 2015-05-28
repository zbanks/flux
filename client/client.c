#include "broker/broker.h"
#include "core/err.h"
#include "server/server.h"
#include "client/client.h"
#include "lib/mdcliapi.h"
#include <czmq.h>

mdcli_t * client;

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

void client_init(int verbose){
    printf("Client starting on localhost:" BROKER_PORT "...\n");

    client = mdcli_new("tcp://localhost:" BROKER_PORT, verbose);
    if(!client) FAIL("Unable to open client\n");
    mdcli_set_timeout(client, 10);
    mdcli_set_retries(client, 3);
}

void client_del(){
    mdcli_destroy(&client);

    printf("Client closed.\n");
}

