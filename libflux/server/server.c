#include "lib/mdwrkapi.h"
#include "flux.h"
#include <czmq.h>

struct _flux_dev {
    mdwrk_t * worker;
    zsock_t * socket;
    flux_id_t name;
    char unused; // Null byte following name. Too paranoid?
    void * args;
    request_fn_t request;
};

#define N_MAX_DEVICES 64

static flux_dev_t devices[N_MAX_DEVICES];
static int n_devices = 0;
static zpoller_t * poller = NULL;
static int poll_interval = 500;
static int verbose = 0;

void flux_server_set_poll_interval(int interval){
    poll_interval = interval;
}

int flux_server_init(int _verbose){
    verbose = _verbose;
    if(verbose) printf("Starting server.\n");

    poller = zpoller_new(NULL);
    if(!poller) return -1;

    for(int i = 0; i < N_MAX_DEVICES; i++){
        if(!devices[i].worker) continue;

        if(zpoller_add(poller, devices[i].socket)){
            zpoller_destroy(&poller);
            return -1;
        }
    }

    return 0;
}

void flux_server_close(){
    // Destroy Resources
    for(int i = 0; i < N_MAX_DEVICES; i++){
        flux_dev_del(&devices[i]);
    }

    zpoller_destroy(&poller);

    if(verbose) printf("Server closed.\n");
}

int flux_server_poll(){
    // Blocks until an event happens
    if(!poller) return -1;

    zsock_t * which = zpoller_wait(poller, poll_interval);
    if(zpoller_terminated(poller)) return -1;
    if(!zpoller_expired(poller) && which){
        for(int i = 0; i < N_MAX_DEVICES; i++){
            if(which == devices[i].socket){
                int rc = -1;
                zmsg_t * body = mdwrk_event(devices[i].worker);
                zmsg_t * reply = NULL;

                if(!body) break;
                if(zmsg_size(body) >= 1){
                    char * cmd = zmsg_popstr(body);
                    rc = devices[i].request(devices[i].args, cmd, body, &reply);
                    zmsg_destroy(&body);
                    free(cmd);
                }

                if(reply){
                    if(!rc) zmsg_pushstr(reply, "200");
                    else zmsg_pushstr(reply, "500");
                }else{
                    reply = zmsg_new();
                    zmsg_pushstr(reply, "500");
                }

                rc = mdwrk_send(devices[i].worker, &reply);
                if(rc && verbose) printf("Error sending reply from worker %16.s\n", devices[i].name);
                
                break;
            }
        }
    }

    for(int i = 0; i < N_MAX_DEVICES; i++){
        if(!devices[i].worker) continue;
        mdwrk_check_heartbeat(devices[i].worker);
    }

    return 0; 
}

flux_dev_t * flux_dev_init(const char * broker, const flux_id_t name, request_fn_t request, void * args){
    assert(name);
    if(n_devices == N_MAX_DEVICES){
        if(verbose){
            printf("Unable to add additional devices: the server only supports %d devices.\n\n", N_MAX_DEVICES);
            printf("You probably shouldn't have this many devices on the same server anyways.\n");
            printf("Think more about your architecture or write your own server.\n\n");
        }
        return NULL;
    }

    // We can't guarentee that devices[] is initialized to 0, but n_devices is.
    // If n_devices == 0, then we don't care about any of the data in devices[]
    if(n_devices == 0) memset(devices, 0, sizeof(flux_dev_t) * N_MAX_DEVICES);

    // Find the first open device. A device is open if the worker pointer is NULL
    flux_dev_t * device = devices;
    while(device->worker) device++;

    memset(device->name, 0, sizeof(flux_id_t));
    strncpy(device->name, name, sizeof(flux_id_t));
    device->unused = 0;
    device->args = args;
    device->request = request;

    // This call never fails
    device->worker = mdwrk_new(broker, device->name, verbose);
    // This API might change
    device->socket = device->worker->worker; 

    if(poller){
        int rc = zpoller_add(poller, device->socket);
        if(rc){ // Unable to add to poller
            mdwrk_destroy(&device->worker);
            memset(device, 0, sizeof(flux_dev_t));
            return NULL;
        }
    }
    n_devices++;

    if(verbose) printf("Connected to broker on %16.s with device %s\n", broker, name);

    return device;
}

void flux_dev_del(flux_dev_t * device){
    if(!device->worker) return;
    if(verbose) printf("Destroying device %16.s\n", device->name);
    if(poller && device->socket) zpoller_remove(poller, device->socket);
    mdwrk_destroy(&device->worker);
    memset(device, 0, sizeof(flux_dev_t));
    n_devices--;
}

