#include "lib/err.h"
#include "serial/serial.h"

#include <flux.h>
#include <czmq.h>

struct device {
    int id;
    flux_id_t name;
    flux_dev_t * flux_dev;
    int bus;
    unsigned int length;
    zhash_t * info;
    char length_str[5];
    char id_str[256];
};

#define DEFAULT_BROKER_URL "tcp://localhost:1365"

#define N_LUX_IDS 4
static int n_lux_ids = N_LUX_IDS;
static uint32_t lux_ids[N_LUX_IDS] = {0x1, 0x2, 0x4, 0x8};

static struct device devices[N_LUX_IDS] = {{0}};
static int serial_available = 0;
static int verbose = 0;
static char * broker_url;

int dummy_request(void * args, const char * cmd, zmsg_t ** body, zmsg_t ** reply){
    UNUSED(args);

    if(strcmp(cmd, "PING") == 0){
        *reply = *body;
        zmsg_pushstr(*reply, "PONG");
    }else if(strcmp(cmd, "ECHO") == 0){
        *reply = *body;
    }else if(strcmp(cmd, "INFO") == 0){
        zmsg_destroy(body);
        zmsg_t * r = *reply = zmsg_new();
        zmsg_pushstr(r, "dummy=dummy");
    }else{
        zmsg_destroy(body);
        return -1;
    }

    return 0;
}

int lux_request(void * args, const char * cmd, zmsg_t ** msg, zmsg_t ** reply){
    struct device * device = (struct device *) args;
    int rc = 0;

    //printf("Req %s: %s\n", device->name, cmd);
    if(strcmp(cmd, "PING") == 0){
        *reply = *msg;
        zmsg_pushstr(*reply, "PONG");
    }else if(strcmp(cmd, "ECHO") == 0){
        *reply = *msg;
    }else if(strcmp(cmd, "INFO") == 0){
        zmsg_destroy(msg);
        zmsg_t * r = *reply = zmsg_new();
        zmsg_push(r, zhash_pack(device->info));
    }else if(strcmp(cmd, "FRAME") == 0){
        *reply = *msg;
        zframe_t * pixels = zmsg_pop(*msg);
        if(zframe_size(pixels) == device->length * 3){
            struct lux_frame lf;

            memcpy(lf.data.carray.data, zframe_data(pixels), device->length * 3);

            lf.data.carray.cmd = CMD_FRAME;
            lf.destination = device->id;
            lf.length = (device->length * 3) + 1;

            rc = lux_tx_packet(&lf);
        }else{
            rc = -1;
        }

        zframe_destroy(&pixels);
    }else{
        rc = -1;
    }

    return rc;
}

static void enumerate_devices(){
    for(int i = 0; i < n_lux_ids; i++){
        struct lux_frame cmd;
        struct lux_frame resp;
        char r;

        cmd.data.raw[0] = CMD_GET_ID;
        cmd.destination = devices[i].id;
        cmd.length = 1;

        if((r = lux_command_response(&cmd, &resp, 100))) goto fail;

        printf("Found light strip %d @0x%08x: '%s'\n", i, cmd.destination, resp.data.raw);
        strncpy(devices[i].id_str, (char *) resp.data.raw, 255);
        zhash_update(devices[i].info, "id", devices[i].id_str);
        devices[i].bus = 1;

        cmd.data.raw[0] = CMD_GET_LENGTH;
        if((r = lux_command_response(&cmd, &resp, 100))) goto fail;

        printf("length: %d\n", resp.data.ssingle_r.data);
        devices[i].length = resp.data.ssingle_r.data;
        snprintf(devices[i].length_str, 4, "%d", resp.data.ssingle_r.data);
        zhash_update(devices[i].info, "length", devices[i].length_str);
        devices[i].bus = 1;
        cmd.length = 1;

fail:
        if(r){
            printf("failed cmd: %d\n", r);
            devices[i].bus = 0;
        }

        if(devices[i].bus && !devices[i].flux_dev){
            devices[i].flux_dev = flux_dev_init(broker_url, devices[i].name, &lux_request, (void *) &devices[i]);
        }else if(!devices[i].bus && devices[i].flux_dev){
            flux_dev_del(devices[i].flux_dev);
            devices[i].flux_dev = 0;
        }
    }
}


int main(int argc, char ** argv){
    char * dummy_name = NULL;
    flux_dev_t * dummy_dev = NULL;

    argv++; argc--;
    if(argc) broker_url = *argv++, argc--;
    if(argc) dummy_name = *argv++, argc--;
    if(argc) verbose = streq(*argv++, "-v"), argc--;
    if(dummy_name && streq(dummy_name, "-v")){
        verbose = 1;
        dummy_name = NULL;
    }

    if(dummy_name) dummy_dev = flux_dev_init(broker_url, dummy_name, &dummy_request, NULL);

    for(int i = 0; i < n_lux_ids; i++){
        snprintf(devices[i].name, 15, "lux:%08X", lux_ids[i]);
        devices[i].id = lux_ids[i];
        devices[i].bus = 0;
        devices[i].info = zhash_new();
    }

    if(!serial_init()) fprintf(stderr, "Unable to initialize serial port.\n");
    else serial_available = 1;
    if(!serial_available && !dummy_dev) FAIL("No devices to broadcast; exiting.\n");

    if(flux_server_init(verbose)) FAIL("Unable to intialize flux server.\n");

    int rc = 0;
    while(!rc){
        if(serial_available) enumerate_devices();
        for(int i = 0; i < 1000; i++)
            if((rc = flux_server_poll())) break;
    }

    // Teardown
    if(serial_available) serial_close();

    flux_server_close();

    if(dummy_dev) flux_dev_del(dummy_dev);

    for(int i = 0; i < n_lux_ids; i++){
        zhash_destroy(&devices[i].info);
    }
}
