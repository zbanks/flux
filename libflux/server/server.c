#include "lib/err.h"
#include "lib/mdwrkapi.h"
#include "flux.h"
#include <czmq.h>

struct resource {
    mdwrk_t * worker;
    zsock_t * socket;
    flux_id_t name;
    char unused; // Null byte following name. Too paranoid?
    void * args;
    int verbose;
    request_fn_t request;
};

#define N_MAX_RESOURCES 64

static resource_t resources[N_MAX_RESOURCES];
static int n_resources = 0;
static zpoller_t * poller = NULL;
static int poll_interval = 500;

void server_set_poll_interval(int interval){
    poll_interval = interval;
}

void server_init(){
    printf("Starting server.\n");

    poller = zpoller_new(NULL);
    if(!poller) FAIL("Unable to open ZMQ poller.\n");

    for(int i = 0; i < N_MAX_RESOURCES; i++){
        if(!resources[i].worker) continue;
        int rc;
        rc = zpoller_add(poller, resources[i].socket);
        if(rc) FAIL("Unable to set up ZMQ poller.\n");
    }
}

void server_del(){
    // Destroy Resources
    for(int i = 0; i < N_MAX_RESOURCES; i++){
        server_rm_resource(&resources[i]);
    }
    if(n_resources) printf("Error destroying server resources, %d left.\n", n_resources);

    zpoller_destroy(&poller);

    printf("Server closed.\n");
}

int server_run(){
    if(!poller) server_init();

    zsock_t * which = zpoller_wait(poller, poll_interval);
    if(zpoller_terminated(poller)) return -1;
    if(!zpoller_expired(poller) && which){
        for(int i = 0; i < N_MAX_RESOURCES; i++){
            if(which == resources[i].socket){
                int rc;
                zmsg_t * msg = mdwrk_event(resources[i].worker);
                zmsg_t * reply = NULL;

                if(!msg) break;
                if(zmsg_size(msg) >= 1){
                    char * cmd = zmsg_popstr(msg);
                    rc = resources[i].request(resources[i].args, cmd, msg, &reply);
                    zmsg_destroy(&msg);
                    free(cmd);
                }

                if(reply){
                    if(!rc) zmsg_pushstr(reply, "200");
                    else zmsg_pushstr(reply, "500");
                }else{
                    reply = zmsg_new();
                    zmsg_pushstr(reply, "500");
                }

                rc = mdwrk_send(resources[i].worker, &reply);
                if(rc) printf("Error sending reply from worker %16.s\n", resources[i].name);
                
                break;
            }
        }
    }

    for(int i = 0; i < N_MAX_RESOURCES; i++){
        if(!resources[i].worker) continue;
        mdwrk_check_heartbeat(resources[i].worker);
    }

    return 0; 
}

resource_t * server_add_resource(char * broker, flux_id_t name, request_fn_t request, void * args, int verbose){
    assert(name);
    if(n_resources == N_MAX_RESOURCES){
        printf("Unable to add additional resource: the server only supports %d resources.\n\n", N_MAX_RESOURCES);
        printf("You probably shouldn't have this many resources on the same server anyways.\n");
        printf("Think more about your architecture or write your own server.\n\n");
        assert(n_resources < N_MAX_RESOURCES);
        return NULL;
    }

    resource_t * resource = resources;
    while(resource->worker) resource++;

    memset(resource->name, 0, sizeof(flux_id_t));
    strncpy(resource->name, name, sizeof(flux_id_t));
    resource->unused = 0;
    resource->verbose = verbose;
    resource->args = args;
    resource->request = request;

    resource->worker = mdwrk_new(broker, resource->name, resource->verbose);
    // This API might change
    resource->socket = resource->worker->worker; 


    if(poller){
        int rc;
        rc = zpoller_add(poller, resource->socket);
        if(rc){ // Unable to add to poller
            mdwrk_destroy(&resource->worker);
            memset(resource, 0, sizeof(resource_t));
            return NULL;
        }
    }
    n_resources++;

    printf("Connected to broker on %16.s with resource %s\n", broker, name);

    return resource;
}

void server_rm_resource(resource_t * resource){
    if(!resource->worker) return;
    printf("Destroying resource %16.s\n", resource->name);
    if(poller && resource->socket) zpoller_remove(poller, resource->socket);
    mdwrk_destroy(&resource->worker);
    if(resource->worker) printf("Unable to destroy worker?\n");
    memset(resource, 0, sizeof(resource_t));
    n_resources--;
}

zsock_t * server_resource_get_socket(resource_t * resource){
    return resource->socket;
}
