#include "broker/broker.h"
#include "core/err.h"
#include "lib/mdwrkapi.h"
#include "server/server.h"
#include <czmq.h>

static struct resource resources[N_MAX_RESOURCES];
static int n_resources = 0;
static zpoller_t * poller = NULL;

#define POLL_INTERVAL 500

void server_init(){
    printf("Starting server.\n");

    poller = zpoller_new(NULL);
    if(!poller) FAIL("Unable to open ZMQ poller.\n");

    for(int i = 0; i < n_resources; i++){
        int rc;
        rc = zpoller_add(poller, resources[i].socket);
        if(rc) FAIL("Unable to set up ZMQ poller.\n");
    }
}

void server_del(){
    // Destroy Resources
    for(int i = 0; n_resources && i < N_MAX_RESOURCES; i++){
        server_del_resource(resources);
    }
    if(n_resources) printf("Error destroying server resources.\n");

    zpoller_destroy(&poller);

    printf("Server closed.\n");
}

void server_run(){
    int rc;

    if(!poller) server_init();

    while(1){
        zsock_t * which = zpoller_wait(poller, POLL_INTERVAL);
        if(zpoller_terminated(poller)) break;
        if(!zpoller_expired(poller)){
            for(int i = 0; i < n_resources; i++){
                if(which == resources[i].socket){
                    zmsg_t * msg = mdwrk_event(resources[i].worker);
                    if(msg){
                        zmsg_t * reply = resources[i].request(&resources[i], msg);
                        rc = mdwrk_send(resources[i].worker, &reply);
                        if(!rc) printf("Error sending reply from worker %s\n", resources[i].name);
                    }
                    break;
                }
            }
        }

        for(int i = 0; i < n_resources; i++){
            mdwrk_check_heartbeat(resources[i].worker);
        }
    }
    
}

static zmsg_t * lux_request(struct resource * resource, zmsg_t * msg){
    printf("Recieved lux request on %08x\n / %s", resource->id, resource->name);
    return msg;
}

struct resource * server_add_lux_resource(int id, int verbose){
    if(n_resources == N_MAX_RESOURCES) return NULL;
    struct resource * resource = &resources[n_resources++];

    resource->id = id;
    resource->verbose = verbose;
    resource->name = malloc(16);
    if(!resource->name){
        n_resources--;
        return NULL;
    }
    snprintf(resource->name, 15, "lux:%08X", id);

    resource->worker = mdwrk_new("tcp://localhost:" BROKER_PORT, resource->name, resource->verbose);
    // This API might change
    resource->socket = resource->worker->worker; 

    resource->request = lux_request;

    if(poller){
        int rc;
        rc = zpoller_add(poller, resource->socket);
        if(rc){ // Unable to add to poller
            n_resources--;
            mdwrk_destroy(&resource->worker);
            free(resource->name);
            return NULL;
        }
    }

    return resource;
}

void server_del_resource(struct resource * resource){
    printf("Destroying resource %s\n", resource->name);
    if(poller && resource->socket) zpoller_remove(poller, resource->socket);
    mdwrk_destroy(&resource->worker);
    if(resource->worker) printf("Unable to destroy worker?\n");
    free(resource->name);
    for(int i = 0; i < n_resources; i++){
        if(&resources[i] == resource){
            n_resources--;
            memcpy(&resources[i], &resources[i+1], sizeof(struct resource) * (n_resources - i));
            memset(&resources[n_resources], 0, sizeof(struct resource));
            break;
        }
    }
}
