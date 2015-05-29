#include "lib/err.h"
#include "lib/mdcliapi.h"
#include "flux.h"
#include <czmq.h>

struct client {
    mdcli_t * mdcli;
    int verbose;
};

int client_id_list(client_t * client, char * prefix, flux_id_t ** ids){
    int n;
    assert(client);

    zmsg_t * list_msg = zmsg_new();
    zmsg_t * reply_msg;

    if(client_send(client, "mmi.list", prefix ? prefix : "", &list_msg, &reply_msg)) goto fail;
    if(!reply_msg) goto fail;

    n = zmsg_size(reply_msg);

    if(ids){
        flux_id_t * cursor = *ids = malloc(n * sizeof(flux_id_t));
        if(!cursor) goto fail;
        memset(cursor, 0, n * sizeof(flux_id_t));

        zframe_t * frame;
        while((frame = zmsg_pop(reply_msg))){
            memcpy(cursor++, zframe_data(frame), MIN(zframe_size(frame), sizeof(flux_id_t)));
            zframe_destroy(&frame);
        }
    }

    if(0){
fail: 
        n = -1;
    }

    zmsg_destroy(&reply_msg);
    return n;
}

int client_id_check(client_t * client, char * prefix){
    assert(client);
    assert(prefix);

    zmsg_t * list_msg = zmsg_new();
    zmsg_t * reply_msg;

    int rc = client_send(client, "mmi.service", prefix, &list_msg, &reply_msg);
    zmsg_destroy(&reply_msg);

    return rc;
}

int client_send(client_t * client, char * name, char * cmd, zmsg_t ** msg, zmsg_t ** reply){
    int rc = -1;
    assert(client);
    assert(name);
    assert(cmd);
    assert(msg);
    assert(reply);

    zmsg_pushstr(*msg, cmd);
    *reply = mdcli_send(client->mdcli, name, msg);
    if(!*reply) return -1;

    zframe_t * code = zmsg_pop(*reply);
    if(zframe_streq(code, "200")) rc = 0;
    else rc = -1;
    zframe_destroy(&code);

    return rc;
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

