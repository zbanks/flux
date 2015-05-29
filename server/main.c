#include "lib/err.h"
#include "serial/serial.h"

#include <flux.h>
#include <czmq.h>

#define BROKER_URL "tcp://localhost:5555"
#define N_LUX_IDS 4

static int n_lux_ids = N_LUX_IDS;
static uint32_t lux_ids[N_LUX_IDS] = {0x1, 0x2, 0x4, 0x8};
static int verbose = 0;

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

    printf("Req %s: %s\n", device->name, cmd);
    if(strcmp(cmd, "PING") == 0){
        zmsg_t * r = *reply = zmsg_new();
        zmsg_pushstr(r, "PONG");
    }else if(strcmp(cmd, "ECHO") == 0){
        *reply = zmsg_dup(msg);
    }else if(strcmp(cmd, "INFO") == 0){
        zmsg_t * r = *reply = zmsg_new();
        zmsg_push(r, zhash_pack(device->info));
    }else if(strcmp(cmd, "FRAME") == 0){
        zframe_t * pixels = zmsg_last(msg);
        UNUSED(pixels);
        printf("Frame!\n");  
    }

    return 0;
}

static void enumerate_devices(){
    for(int i = 0; i < n_lux_ids; i++){
        if(devices[i].bus && !devices[i].resource){
            devices[i].resource = server_add_resource(BROKER_URL, devices[i].name, &lux_request, (void *) &devices[i], verbose);
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
        devices[i].info = zhash_new();
        zhash_insert(devices[i].info, "key1", "value1");
        zhash_insert(devices[i].info, "key2", "value2");
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
