#include "core/err.h"
#include "server/server.h"
#include "serial/serial.h"

#include <czmq.h>

#define N_LUX_IDS 4

static int n_lux_ids = N_LUX_IDS;
static uint32_t lux_ids[N_LUX_IDS] = {0x1, 0x2, 0x4, 0x8};

struct device {
    uint32_t id;
    char name[16];
    struct resource * resource;
    int bus;
};

static struct device devices[N_LUX_IDS] = {{0}};

zmsg_t * lux_request(struct resource * resource, zmsg_t * msg, void * args){
    struct device * device = (struct device *) args;
    if(zmsg_size(msg) != 2){
        zmsg_destroy(&msg);
        return zmsg_new();
    }

    char needs_data = 0;
    zframe_t * req = zmsg_pop(msg);
    if(zframe_size(req)) needs_data = *(char *) zframe_data(req);

    char * s = zframe_strdup(zmsg_last(msg));
    printf("Recieved lux %u %08x/%s '%s'\n", needs_data, device->id, resource->name, s);
    free(s);

    zmsg_pushmem(msg, "", 0);
    return msg;
}

static void enumerate_devices(){
    for(int i = 0; i < n_lux_ids; i++){
        if(devices[i].bus && !devices[i].resource){
            devices[i].resource = server_add_resource(devices[i].name, &lux_request, (void *) &devices[i], 0);
        }else if(!devices[i].bus && devices[i].resource){
            server_rm_resource(devices[i].resource);
            devices[i].resource = 0;
        }
    }
}

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    for(int i = 0; i < n_lux_ids; i++){
        snprintf(devices[i].name, 15, "lux:%08X", lux_ids[i]);
        devices[i].id = lux_ids[i];
        devices[i].bus = 1;
    }

    //if(!serial_init()) FAIL("Unable to initialize serial port.\n");

    server_init();

    int rc = 0;
    while(!rc){
        enumerate_devices();
        for(int i = 0; i < 1000; i++)
            if((rc = server_run())) break;
    }

    server_del();
}
