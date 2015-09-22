#include "flux.h"
#include <nanomsg/nn.h>
#include <nanomsg/survey.h>
#include <nanomsg/reqrep.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define N_MAX_SERVERS 32

struct flux_server {
    int sock; 
    size_t n_ids;
    flux_id_t * ids;
    char * rep_url;
};

struct _flux_cli {
    int broker_sock;
    int timeout;
    int verbose;
    size_t n_servers;
    struct flux_server * servers;
};

static char * servers_string = NULL;

int flux_cli_id_list(flux_cli_t * client, flux_id_t ** ids){
    assert(ids);
    assert(*ids == NULL);

    int id_size = nn_send(client->broker_sock, "ID", 2, 0);
    if(id_size != 2){
        if(client->verbose) printf("Unable to list ids: unable to send to broker: %s\n", nn_strerror(errno));
        return -1;
    }
    
    char * resp = NULL;
    int resp_size = nn_recv(client->broker_sock, &resp, NN_MSG, 0);
    if(resp_size < 0){
        if(client->verbose) printf("Unable to list ids: invalid broker response: %s\n", nn_strerror(errno));
        return -1;
    }

    if(servers_string) free(servers_string);
    servers_string = malloc(resp_size);
    assert(servers_string);
    memcpy(servers_string, resp, resp_size);
    nn_freemsg(resp);

    // Split lines & tokenize. Each newline is a server
    // Tokenize string in form "URL\tIDS\n"; replace '\t' with '\0'
    
    // XXX: not quite thread safe
    int n = 0;
    for(int i = 0; i < resp_size; i++){
        if(servers_string[i] == '\n') n++;
    }

    struct flux_server * servers = NULL;
    if(n > 0){
        if(servers_string[resp_size-1] != '\n'){
            if(client->verbose) printf("Warning while listing ids: invalid broker response.\n");
            return -1;
        }
        servers = malloc(n * sizeof(struct flux_server));
        assert(servers);
        char * sptr = servers_string;
        for(int i = 0; i < n; i++){
            servers[i].rep_url = sptr;
            int j = 0;
            while(sptr[j] != '\t' && sptr[j] != '\n') j++;
            if(sptr[j] != '\t'){
                if(client->verbose) printf("Warning while listing ids: invalid broker response.\n");
                if(client->verbose) printf("Msg: %s; %s\n", servers_string, sptr);
                free(servers);
                return -1;
            }
            sptr += j;
            *(sptr++) = '\0';
            servers[i].ids = (flux_id_t *) sptr;
            for(j = 0; sptr[j] != '\n'; j++);
            servers[i].n_ids = j / sizeof(flux_id_t);
            //assert(servers[i].n_ids * sizeof(flux_id_t) == (size_t) j);
            sptr += j;
            *(sptr++) = '\0';

            servers[i].sock = nn_socket(AF_SP, NN_REQ);
            assert(servers[i].sock >= 0);
            assert(nn_setsockopt(servers[i].sock, NN_SOL_SOCKET, NN_RCVTIMEO, &client->timeout, sizeof(int)) >= 0);
            assert(nn_setsockopt(servers[i].sock, NN_SOL_SOCKET, NN_SNDTIMEO, &client->timeout, sizeof(int)) >= 0);
            if(nn_connect(servers[i].sock, servers[i].rep_url) >= 0){
                if(client->verbose) printf("Connected to server: %s\n", servers[i].rep_url);
            }else{
                if(client->verbose) printf("Unable to connect to server %s: %s\n", servers[i].rep_url, nn_strerror(errno));
                assert(nn_close(servers[i].sock) == 0);
                n--;
                i--;
            }
        }
    }

    for(size_t i = 0; i < client->n_servers; i++){
        assert(nn_close(client->servers[i].sock) == 0);
    }
    client->n_servers = n;
    if(client->servers) free(client->servers);
    client->servers = servers;


    // Count the total number of ids
    size_t n_ids = 0;
    for(size_t i = 0; i < client->n_servers; i++){
        n_ids += client->servers[i].n_ids;
    }

    // Allocate a buffer to id array
    *ids = malloc(n_ids * sizeof(flux_id_t));
    assert(*ids);

    // Concatenate ids
    char * iptr = (char *) *ids;
    for(size_t i = 0; i < client->n_servers; i++){
        memcpy(iptr, client->servers[i].ids, client->servers[i].n_ids * sizeof(flux_id_t));
        iptr += sizeof(flux_id_t);
    }

    return n_ids;
}

void flux_buffer_del(void * buffer){
    free(buffer);
}

int flux_cli_send(flux_cli_t * client, const flux_id_t dest, const flux_cmd_t cmd, const char * body, size_t body_size, char ** reply){
    assert(client);
    assert(dest);
    assert(cmd);
    assert(body || body_size == 0);
    assert(reply); // We need a spot to put the reply...
    assert(*reply == NULL); // ...but there shouldn't  already be something there!

    int req_sock = -1;

    for(size_t i = 0; i < client->n_servers; i++){
        for(size_t j = 0; j < client->servers[i].n_ids; j++){
            if(memcmp(&client->servers[i].ids[j], dest, sizeof(flux_id_t)) == 0){
                req_sock = client->servers[i].sock;
                goto found_dest_match;
            }
        }
    }
    if(client->verbose) printf("No ID found matching %16s\n in %u servers", dest, (unsigned int) client->n_servers);
    return -1;

found_dest_match:
    assert(req_sock >= 0);

    // Build message by concatenating dest, cmd, and body
    int msg_size = sizeof(flux_id_t) + sizeof(flux_cmd_t) + body_size;
    char * msg = malloc(msg_size);
    assert(msg);
    char * mptr = msg;
    memcpy(mptr, dest, sizeof(flux_id_t)); mptr += sizeof(flux_id_t);
    memcpy(mptr, cmd, sizeof(flux_cmd_t)); mptr += sizeof(flux_cmd_t);
    if(body_size){
        memcpy(mptr, body, body_size); 
        mptr += body_size;
    }
    //assert(mptr - msg == msg_size);

    // Send `msg` & recieve `resp`  (REQ/REP)
    int msg_send = nn_send(req_sock, msg, msg_size, NN_DONTWAIT);
    if(msg_send != msg_size){
        if(client->verbose) printf("Error sending message: %s\n", nn_strerror(errno));
        return -1;
    }

    char * resp;
    int resp_size = nn_recv(req_sock, &resp, NN_MSG, 0);
    if(resp_size < 0){
        if(client->verbose) printf("Error receiving message: %s\n", nn_strerror(errno));
        return -1;
    }
    if(client->verbose) printf("Recieved %d bytes\n", resp_size);

    // Repackage response so it can be free'd
    *reply = malloc(resp_size);
    assert(*reply);
    memcpy(*reply, resp, resp_size);
    nn_freemsg(resp);

    return resp_size;
}

flux_cli_t * flux_cli_init(const char * broker_url, int timeout, int verbose){
    flux_cli_t * client;
    if(verbose) printf("Client starting on %s...\n", broker_url);

    client = malloc(sizeof(flux_cli_t));
    if(!client) return NULL;
    memset(client, 0, sizeof(flux_cli_t));

    client->verbose = verbose;
    client->timeout = timeout;
    client->n_servers = 0;
    client->servers = NULL;

    // Bind to broker SURVEYOR socket
    client->broker_sock =  nn_socket(AF_SP, NN_REQ);
    assert(client->broker_sock >= 0);
    assert(nn_setsockopt(client->broker_sock, NN_SOL_SOCKET, NN_RCVTIMEO, &client->timeout, sizeof(int)) >= 0);
    assert(nn_setsockopt(client->broker_sock, NN_SOL_SOCKET, NN_SNDTIMEO, &client->timeout, sizeof(int)) >= 0);
    if(nn_connect(client->broker_sock, broker_url) < 0){
        printf("Unable to init flux client: %s\n", nn_strerror(errno));
        return NULL;
    }

    return client;
}

void flux_cli_del(flux_cli_t * client){
    if(client->verbose) printf("Client closed.\n");

    for(size_t i = 0; i < client->n_servers; i++){
        nn_shutdown(client->servers[i].sock, 0);
    }

    client->n_servers = 0;
    nn_shutdown(client->broker_sock, 0);
    free(servers_string);

    free(client);
}
