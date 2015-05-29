#include "lib/err.h"
#include "serial/serial.h"

#include <flux.h>
#include <czmq.h>

#define BROKER_URL "tcp://localhost:5555"
#define N_LUX_IDS 4

static int n_lux_ids = N_LUX_IDS;
static uint32_t lux_ids[N_LUX_IDS] = {0x1, 0x2, 0x4, 0x8};

struct device {
    uint32_t id;
    flux_id_t name;
    resource_t * resource;
    int bus;
    zhash_t * info;
};

static struct device devices[N_LUX_IDS] = {{0}};

int lux_request(void * args, const char * cmd, zmsg_t * msg, zmsg_t ** reply){
    struct device * device = (struct device *) args;

    char * s = zframe_strdup(zmsg_last(msg));
    printf("Req %s: %s '%s'\n", device->name, cmd, s);
    free(s);

    *reply = zmsg_dup(msg);
    return 0;
}

static void enumerate_devices(){
    for(int i = 0; i < n_lux_ids; i++){
        if(devices[i].bus && !devices[i].resource){
            devices[i].resource = server_add_resource(BROKER_URL, devices[i].name, &lux_request, (void *) &devices[i], 0);
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
        zhash_new(devices[i].info);
    }

    //if(!serial_init()) FAIL("Unable to initialize serial port.\n");

    server_init();

    int rc = 0;
    while(!rc){
        enumerate_devices();
        for(int i = 0; i < 1000; i++)
            if((rc = server_run())) break;
    }

    // Teardown
    server_del();
    for(int i = 0; i < n_lux_ids; i++){
        zhash_destroy(&devices[i].info);
    }
}
