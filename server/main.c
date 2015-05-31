#include "lib/err.h"
#include "serial/serial.h"

#include <czmq.h>
#include <flux.h>

struct device {
    int id;
    flux_id_t name;
    flux_dev_t * flux_dev;
    ser_t * bus;
    unsigned int length;
    zhash_t * info;
    char length_str[5];
    char id_str[256];
};

#define DEFAULT_BROKER_URL "tcp://localhost:1365"

#define N_LUX_IDS 4
static int n_lux_ids = N_LUX_IDS;
static uint32_t lux_ids[N_LUX_IDS] = {0x1, 0x2, 0x4, 0x8};
static int lux_default_lengths[N_LUX_IDS] = {60, 60, 60, 60}; // For write-only mode

static struct device devices[N_LUX_IDS] = {{0}};
static int write_only = 0;
static int verbose = 0;
static char * broker_url = DEFAULT_BROKER_URL;

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
            rc = lux_tx_packet(device->bus, &lf);
        }else{
            rc = -1;
        }

        zframe_destroy(&pixels);
    }else{
        rc = -1;
    }

    return rc;
}

static void enumerate_devices(ser_t * serial){
    for(int i = 0; i < n_lux_ids; i++){
        if(serial->write_only){
            devices[i].bus = serial;
            devices[i].length = lux_default_lengths[i];
        }else{
            struct lux_frame cmd;
            struct lux_frame resp;
            char r;

            cmd.data.raw[0] = CMD_GET_ID;
            cmd.destination = devices[i].id;
            cmd.length = 1;

            if((r = lux_command_response(serial, &cmd, &resp, 40))) goto sfail;

            printf("Found light strip %d @0x%08x: '%s'\n", i, cmd.destination, resp.data.raw);
            strncpy(devices[i].id_str, (char *) resp.data.raw, 255);
            zhash_update(devices[i].info, "id", devices[i].id_str);
            devices[i].bus = serial;

            cmd.data.raw[0] = CMD_GET_LENGTH;
            if((r = lux_command_response(serial, &cmd, &resp, 40))) goto sfail;

            printf("length: %d\n", resp.data.ssingle_r.data);
            devices[i].length = resp.data.ssingle_r.data;
            snprintf(devices[i].length_str, 4, "%d", resp.data.ssingle_r.data);
            zhash_update(devices[i].info, "length", devices[i].length_str);
            devices[i].bus = serial;
            cmd.length = 1;

sfail:
            if(r){
                printf("failed cmd: %d\n", r);
                devices[i].bus = NULL;
            }
        }

        if(devices[i].bus && !devices[i].flux_dev){
            printf("Adding device: %s\n", devices[i].name);
            devices[i].flux_dev = flux_dev_init(broker_url, devices[i].name, &lux_request, (void *) &devices[i]);
        }else if(!devices[i].bus && devices[i].flux_dev){
            flux_dev_del(devices[i].flux_dev);
            devices[i].flux_dev = 0;
        }
    }
}

static void print_help(){
    printf("Usage: flux-server [-b broker_socket] [-d dummy_id] [-r] [-v]\n"
           " -b broker_socket  Set the ZMQ socket to connect to the broker.\n"
           "                   Default: '" DEFAULT_BROKER_URL "'.\n"
           " -d dummy_id       Create a 'dummy' mock device with the given id.\n"
           " -r                Lux write-only. Do not attempt to read from the lux bus.\n"
           " -v                Verbose\n");
}


int main(int argc, char ** argv){
    char * dummy_name = NULL;
    flux_dev_t * dummy_dev = NULL;
    ser_t * serial = NULL;

    char ** arg_dest = NULL;
    while(--argc){
        argv++; // Chomp the next argument. We ignore the first argument (the program name)
        if(argv[0][0] == '-'){
            if(arg_dest){
                print_help();
                fprintf(stderr, "Expected argument value\n");
                return -1;
            }
            switch(argv[0][1]){
                case 'b': arg_dest = &broker_url; break;
                case 'd': arg_dest = &dummy_name; break;
                case 'v': verbose = 1; break;
                case 'r': write_only = 1; break;
                case 'h': print_help(); return 0;
                default:
                    print_help();
                    fprintf(stderr, "Invalid argument '%s'\n", *argv);
                    return -1;
            }
        }else{
            if(!arg_dest){
                print_help();
                fprintf(stderr, "Invalid argument '%s'\n", *argv);
                return -1;
            }
            *arg_dest = *argv;
            arg_dest = NULL;
        }
    }
    if(arg_dest){
        print_help();
        fprintf(stderr, "Expected argument value\n");
        return -1;
    }


    serial = serial_init(write_only);
    if(!serial) fprintf(stderr, "Unable to initialize serial port.\n");

    for(int i = 0; i < n_lux_ids; i++){
        snprintf(devices[i].name, 15, "lux:%08X", lux_ids[i]);
        devices[i].id = lux_ids[i];
        devices[i].bus = NULL;
        devices[i].info = zhash_new();
    }

    if(dummy_name) dummy_dev = flux_dev_init(broker_url, dummy_name, &dummy_request, NULL);
    if(!serial && !dummy_dev) FAIL("No devices to broadcast; exiting.\n");

    if(flux_server_init(verbose)) FAIL("Unable to intialize flux server.\n");

    int rc = 0;
    while(!rc){
        if(serial) enumerate_devices(serial);
        for(int i = 0; i < 1000; i++)
            if((rc = flux_server_poll())) break;
    }

    // Teardown
    if(serial) serial_close(serial);

    flux_server_close();

    if(dummy_dev) flux_dev_del(dummy_dev);

    for(int i = 0; i < n_lux_ids; i++){
        zhash_destroy(&devices[i].info);
    }
}
