#include "flux.h"
#include <nanomsg/nn.h>

#define N_MAX_SERVERS 32

static struct flux_server {
    int sock; 
    int n_ids;
    flux_id_t * ids;
    char * rep_url;
};

struct _flux_cli {
    int broker_sock;
    int verbose;
    int n_servers;
    int survey_state;
    struct flux_server servers[N_MAX_SERVERS];
    struct flux_server survey_responses[N_MAX_SERVERS];
};

int flux_cli_survey(flux_cli_t * client){
    if(client->survey_state >= 0) return -1;
    client->survey_state = 0;

    int id_size = nn_send(client->broker_sock, "ID", 2, 0);
    assert(id_size == 2);

    while(1){
        char * resp = NULL;
        int resp_size = nn_recv(client->broker_sock, &resp, NN_MSG, 0);
        if(resp_size == ETIMEOUT) break;
        if(resp_size >= 0){
            char * tokbuf;
            client->rep_url = strtok_r(resp, "|", &tokbuf);
            client->ids = strtok_r(0, "|", &tokbuf);
            client->n_ids = strlen(client->ids) / sizeof(flux_id_t);
            assert(client->n_ids * sizeof(flux_id_t) == strlen(client->ids));
            assert(strtok_r(0, "|", &tokbuf) == NULL);

            client->sock = nn_socket(AF_SP, NN_REQ);
            assert(client->sock >= 0);
            assert(nn_bind(client->sock, client->rep_url) >= 0);

            client->survey_state++;
            nn_freemsg(resp);
        }
    }

    // XXX: Should there be a better lock here?
    int n = client->n_servers;
    client->n_servers = 0;
    for(int i = 0; i < n; i++){
        free(client->servers[i].rep_url);
        free(client->servers[i].ids);
        client->servers[i].n_ids = 0;
        nn_shutdown(client->servers[i].sock, 0);
    }
    memcpy(client->servers, client->survey_responses, client->survey_state * sizeof(struct flux_server));
    client->n_servers = client->survey_state;
    client->survey_state = -1;
}

int flux_cli_id_list(flux_cli_t * client, flux_id_t ** ids){
    int n;
    assert(client);

    // Count the total number of ids
    int n_ids = 0;
    for(int i = 0; i < n_servers; i++){
        n_ids += client->servers[i].n_ids;
    }

    // Allocate a buffer to id array
    assert(*ids == NULL);
    *ids = malloc(n_ids * sizeof(flux_id_t));
    assert(*ids);

    // Concatenate ids
    char * iptr = *ids;
    for(int i = 0; i < client->n_servers; i++){
        memcpy(iptr, client->servers[i].ids, client->servers[i].n_ids * sizeof(flux_id_t));
        iptr += sizeof(flux_id_t);
    }

    return n_ids;
}

int flux_cli_send(flux_cli_t * client, const lux_id_t dest, const lux_cmd_t cmd, char * body, int body_size, char ** reply){
    int rc = -1;
    assert(client);
    assert(dest);
    assert(cmd);
    assert(body || body_size == 0);
    assert(body_size >= 0);
    assert(reply); // We need a spot to put the reply...
    assert(*reply == NULL); // ...but there shouldn't  already be something there!

    int req_sock = -1;

    for(int i = 0; i < client->n_servers; i++){
        for(int j = 0; j < client->servers[i].n_ids; j++){
            if(memcmp(&client->servers[i].ids[j], dest, sizeof(lux_id_t)) == 0){
                req_sock = client->servers[i].req_sock;
                goto found_dest_match;
            }
        }
    }
    if(client->verbose) fprintf("No ID found matching %16s\n in %d servers", dest, client->n_servers);
    return -1;

found_dest_match:
    assert(req_sock >= 0);

    // Build message by concatenating dest, cmd, and body
    int msg_size = sizeof(lux_id_t) + sizeof(lux_cmd_t) + body_size;
    char * msg = malloc(msg_size);
    assert(msg);
    char * mptr = msg;
    memcpy(mptr, dest, sizeof(lux_id_t)); mptr += sizeof(lux_id_t);
    memcpy(mptr, cmd, sizeof(lux_cmd_t)); mptr += sizeof(lux_cmd_t);
    if(body_size){
        memcpy(mptr, body, body_size); 
        mptr += body_size;
    }
    assert(mptr - msg == msg_size);

    // Send `msg` & recieve `resp`  (REQ/REP)
    int msg_send nn_send(req_sock, msg, msg_size, 0);
    assert(msg_send == msg_size);

    char * resp;
    int resp_size = nn_recv(req_sock, &resp, NN_MSG, 0);
    assert(resp_size >= 0);

    // Repackage response so it can be free'd
    *reply = malloc(resp_size);
    assert(*reply);
    memcpy(*reply, resp, resp_size);
    nn_freemsg(resp);

    return resp_size;
}

flux_cli_t * flux_cli_init(const char * broker_url, int verbose){
    flux_cli_t * client;
    if(verbose) printf("Client starting on %s...\n", broker_url);

    client = zmalloc(sizeof(flux_cli_t));
    if(!client) return NULL;

    client->verbose = verbose;
    client->n_servers = 0;
    client->survey_state = -1;

    // Bind to broker SURVEYOR socket
    client->broker_sock =  nn_socket(AF_SP, NN_SURVEYOR);
    assert(client->broker_sock >= 0);
    assert(nn_bind(client->broker_sock, broker_url) >= 0);

    return client;
}

void flux_cli_del(flux_cli_t * client){
    if(client->verbose) printf("Client closed.\n");

    for(int i = 0; i < n; i++){
        free(client->servers[i].rep_url);
        free(client->servers[i].ids);
        client->servers[i].n_ids = 0;
        nn_shutdown(client->servers[i].sock, 0);
    }

    client->n_servers = 0;
    nn_shutdown(client->broker_sock);

    free(client);
}
