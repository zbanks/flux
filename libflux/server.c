#include "flux.h"
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/survey.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/*
 *  Server connects to broker over a SURVEY channel as a RESPONDENT
 *  which allows the broker to collect which ids are available
 *
 *  Server publishes a REP socket.
 *
 */

// TODO: linked list this
struct _flux_dev {
    int exists;
    flux_id_t name;
    char unused; // Null byte following name. Too paranoid?
    void * args;
    request_fn_t request;
};

#define N_MAX_DEVICES 64

static flux_dev_t devices[N_MAX_DEVICES] = {{0}};
static int n_devices = 0;
static int poll_interval = 500;
static int verbose = 0;

static int broker_sock = -1;
static int rep_sock = -1;
static char * rep_url = NULL;
static char * id_response = NULL;
static int id_response_size = 0;
static struct nn_pollfd pollfd[2];

void flux_server_set_poll_interval(int interval){
    poll_interval = interval;
}

int flux_server_init(const char * broker_url, const char * _rep_url, int _verbose){
    verbose = _verbose;
    if(verbose) printf("Starting server.\n");

    // Connect to broker as RESPONDENT
    broker_sock = nn_socket(AF_SP, NN_RESPONDENT);
    assert(broker_sock >= 0);
    assert(nn_connect(broker_sock, broker_url) >= 0);

    // Set up REP socket
    rep_sock = nn_socket(AF_SP, NN_REP);
    assert(rep_sock >= 0);
    assert(nn_bind(rep_sock, _rep_url) >= 0);

    // Set up poll struct to listen for incoming packets
    pollfd[0].fd = broker_sock;
    pollfd[0].events = NN_POLLIN;
    pollfd[1].fd = rep_sock;
    pollfd[1].events = NN_POLLIN;

    // Set up standard responses
    id_response_size = strlen(_rep_url);

    rep_url = malloc(id_response_size + 1);
    memcpy(rep_url, _rep_url, id_response_size);
    rep_url[id_response_size] = '\0';

    id_response = malloc(id_response_size + 1);
    memcpy(id_response, rep_url, id_response_size + 1);

    /*
    for(int i = 0; i < N_MAX_DEVICES; i++){

    }
    */

    return 0;
}

void flux_server_close(){
    // Destroy Resources
    for(int i = 0; i < N_MAX_DEVICES; i++){
        flux_dev_del(&devices[i]);
    }

    // Shut down nanomsg sockets
    nn_shutdown(broker_sock, 0);
    nn_shutdown(rep_sock, 0);

    free(rep_url);
    free(id_response);
    id_response_size = 0;

    if(verbose) printf("Server closed.\n");
}

int flux_server_poll(){
    assert(pollfd[0].events | pollfd[1].events);
    // Blocks until an event happens
    int res = nn_poll(pollfd, 2, poll_interval);
    if(verbose) printf("poll:%d\n", res);
    if(res <= 0) return res; // Return -1 on error; 0 if timeout

    if(pollfd[0].revents & NN_POLLIN){
        assert(pollfd[0].fd == broker_sock);
        char * survey_body = NULL;
        int survey_size = nn_recv(broker_sock, &survey_body, NN_MSG, NN_DONTWAIT);
        if(survey_size < 0){
             // Handle error TODO
             if(verbose) fprintf(stderr, "Error on RESPONDENT socket: %d", survey_size);
        }else{
            if(survey_size == 2 && strncmp(survey_body, "ID", 2) == 0){
                // Respond to ID request
                int id_sent = nn_send(broker_sock, id_response, id_response_size, 0);
                assert(id_sent == id_response_size);
            }
            nn_freemsg(survey_body);
        }
    }

    if(pollfd[1].revents & NN_POLLIN){
        assert(pollfd[1].fd == rep_sock);
        char * msg = NULL;
        int msg_size = nn_recv(rep_sock, &msg, NN_MSG, NN_DONTWAIT);
        if(msg_size < (int) (sizeof(flux_id_t) + sizeof(flux_cmd_t))){
             // Handle error TODO
             if(verbose) fprintf(stderr, "Error on REP socket: %d", msg_size);
        }else{
            // XXX 
            char * msg_id = msg;
            char * msg_cmd = msg + sizeof(flux_id_t);
            char * msg_body = msg + sizeof(flux_id_t) + sizeof(flux_cmd_t);
            msg_size -= sizeof(flux_id_t) + sizeof(flux_cmd_t);

            for(int i = 0; i < N_MAX_DEVICES; i++){
                if(!devices[i].exists) continue;
                if(memcmp(msg_id, devices[i].name, sizeof(flux_id_t))) continue;

                // TODO: pass on errors from `request(...)` 
                char * rep_body;
                int rep_size = devices[i].request(devices[i].args, msg_cmd, msg_body, msg_size, &rep_body);
                if(rep_size >= 0){
                    printf("Sending %d bytes\n", rep_size);
                    int rep_sent = nn_send(rep_sock, rep_body, rep_size, 0);
                    assert(rep_sent == (int) rep_size);
                    //TODO XXX free(rep_body)???
                }
            }
            nn_freemsg(msg);
        }
    }
    return res; 
}

static void rebuild_id_response(){
    // Reconfigure available ids
    assert(rep_url);
    free(id_response);
    size_t rep_size = strlen(rep_url);
    id_response_size = rep_size + n_devices * sizeof(flux_id_t) + 1;
    char * iptr = id_response = malloc(id_response_size);
    assert(id_response);

    memcpy(iptr, rep_url, rep_size); 
    iptr += rep_size;
    *iptr++ = '\t';
    for(int i = 0; i < N_MAX_DEVICES; i++){
        if(!devices[i].exists) continue;
        memcpy(iptr, devices[i].name, sizeof(flux_id_t)); 
        iptr += sizeof(flux_id_t);
    }
    assert(id_response_size == (iptr - id_response));
}

flux_dev_t * flux_dev_init(const flux_id_t name, request_fn_t request, void * args){
    assert(name);
    if(n_devices == N_MAX_DEVICES){
        if(verbose){
            printf("Unable to add additional devices: the server only supports %d devices.\n\n", N_MAX_DEVICES);
            printf("You probably shouldn't have this many devices on the same server anyways.\n");
            printf("Think more about your architecture or write your own server.\n\n");
        }
        return NULL;
    }

    // Find the first open device. A device is open if the worker pointer is NULL
    flux_dev_t * device = devices;
    while(device->exists) device++;

    memset(device->name, 0, sizeof(flux_id_t));
    strncpy(device->name, name, sizeof(flux_id_t));
    device->unused = 0;
    device->args = args;
    device->request = request;
    device->exists = 1;
    n_devices++;

    rebuild_id_response();

    if(verbose) printf("Added device %s\n", name);

    return device;
}


void flux_dev_del(flux_dev_t * device){
    if(!device->exists) return;
    if(verbose) printf("Destroying device %16.s\n", device->name);
    memset(device, 0, sizeof(flux_dev_t));
    n_devices--;
    rebuild_id_response();
}

