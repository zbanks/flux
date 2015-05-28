#include "lib/err.h"
#include "lib/mdcliapi.h"
#include "flux.h"
#include <czmq.h>

struct client {
    mdcli_t * mdcli;
    int verbose;
};

zmsg_t * client_send(client_t * client, char * name, zmsg_t ** msg){
    return mdcli_send(client->mdcli, name, msg);
}

client_t * client_init(char * broker, int verbose){
    client_t * client;

    if(verbose) printf("Client starting on %s...\n", broker);

    client = malloc(sizeof(client_t));
    if(!client) return NULL;

    client->verbose = verbose;
    client->mdcli = mdcli_new(broker, verbose);
    if(!client->mdcli){
        free(client);
        return NULL;
    }

    mdcli_set_timeout(client->mdcli, 10);
    mdcli_set_retries(client->mdcli, 3);

    return client;
}

void client_del(client_t * client){
    if(client->verbose) printf("Client closed.\n");

    mdcli_destroy(&client->mdcli);
    free(client);

}

