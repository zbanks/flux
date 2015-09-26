#include "lib/err.h"
#include "lib/serial/serial.h"
#include "lib/lux/inc/lux_wire.h"

#include <flux.h>
#include <string.h>

// Example Flux Server - Lux Light Strips

struct device {
    int id;
    flux_id_t name;
    flux_dev_t * flux_dev;
    int bus_fd;
    unsigned int length;
    //zhash_t * info;
    char length_str[5];
    char id_str[256];
};

struct lux_frame {
    uint32_t destination;
    int length;
    union lux_command_frame data;
};

#define DEFAULT_BROKER_URL "tcp://*:1366"
#define DEFAULT_SERVER_URL "tcp://*:1368"

#define N_LUX_IDS 5
static int n_lux_ids = N_LUX_IDS;
static uint32_t lux_ids[N_LUX_IDS] = {0x1, 0x2, 0x3, 0x4, 0x5};
//static int lux_default_lengths[N_LUX_IDS] = {140, 140, 140}; // For write-only mode

static struct device devices[N_LUX_IDS];
static int write_only = 0;
static int verbose = 0;

int dummy_request(void * args, const flux_cmd_t cmd, char * body, size_t body_size, char ** reply){
    UNUSED(args);
    int reply_size = -1;

    if(strncmp(cmd, "PING", 12) == 0){
        *reply = "PONG";
        reply_size = 4;
    }else if(strncmp(cmd, "ECHO", 12) == 0){
        *reply = body;
        reply_size = body_size;
    }else if(strncmp(cmd, "LENGTH?", 12) == 0){
        *reply = "999";
        reply_size = 4;
    }else{
        return -1;
    }
    if(verbose) printf("Responding to dummy request: %16s\n", cmd);

    return reply_size;
}

int lux_request(void * args, const flux_cmd_t cmd, char * body, size_t body_size, char ** reply){

    struct device * device = (struct device *) args;
    int rc = -1;

    //printf("Req %s: %s\n", device->name, cmd);
    if(strncmp(cmd, "PING", 16) == 0){
        *reply = "PONG";
        rc = 4;
    }else if(strncmp(cmd, "ECHO", 16) == 0){
        *reply = body;
        rc = body_size;
    }else if(strncmp(cmd, "LENGTH?", 16) == 0){
        *reply = device->length_str;
        rc = sizeof(device->length_str);
    }else if(strncmp(cmd, "FRAME", 16) == 0){
        if(body_size == device->length * 3){
            *reply = "OK";
            rc = 2;

            char frame[2048];
            size_t frame_len = device->length * 3 + 1;
            memcpy(frame+1, body, device->length * 3);
            frame[0] = CMD_FRAME;

            if(lux_write(device->bus_fd, device->id, frame, frame_len))
                rc = -1;
        }else{
            *reply = "BadSize";
            rc = 7;
        }
    }else{
        rc = -1;
    }

    return rc;
}

static void enumerate_devices(int fd)
{
    static char cmd[1024];
    static char resp[1024];

    for(int i = 0; i < n_lux_ids; i++)
    {
        int r;

        if((r = lux_command(fd, devices[i].id, cmd, 0, resp, 3)) < 0) goto sfail;
        resp[r] = 0;

        printf("Found light strip %d @0x%08x: '%s'\n", i, devices[i].id, resp);
        strncpy(devices[i].id_str, (char *) resp, 255);
        //zhash_update(devices[i].info, "id", devices[i].id_str);
        devices[i].bus_fd = fd;

        cmd[0] = CMD_GET_LENGTH;
        if((r = lux_command(fd, devices[i].id, cmd, 1, resp, 3)) < 0) goto sfail;

        uint32_t resp_length;
        memcpy(&resp_length, resp, sizeof(resp_length));
        printf("length: %d\n", resp_length);
        devices[i].length = resp_length;
        snprintf(devices[i].length_str, 4, "%d", resp_length);
        //zhash_update(devices[i].info, "length", devices[i].length_str);
        devices[i].bus_fd = fd;

sfail:
        if(r){
            printf("failed cmd: %d\n", r);
            devices[i].bus_fd = -1;
        }

        if(devices[i].bus_fd >= 0 && !devices[i].flux_dev){
            printf("Adding device: %s\n", devices[i].name);
            devices[i].flux_dev = flux_dev_init(devices[i].name, &lux_request, (void *) &devices[i]);
        }else if(devices[i].bus_fd < 0 && devices[i].flux_dev){
            flux_dev_del(devices[i].flux_dev);
            devices[i].flux_dev = 0;
        }
    }
}

static void print_help(){
    printf("Usage: flux-server [-b broker_socket] [-d dummy_id] [-r] [-v]\n"
           " -b broker_socket  Set the ZMQ socket to connect to the broker.\n"
           "                   Default: '" DEFAULT_BROKER_URL "'.\n"
           " -s server_socket  Set the ZMQ socket for clients to connect to.\n"
           "                   Default: '" DEFAULT_SERVER_URL "'.\n"
           " -d dummy_id       Create a 'dummy' mock device with the given id.\n"
           " -r                Lux write-only. Do not attempt to read from the lux bus.\n"
           " -v                Verbose\n");
}


int main(int argc, char ** argv){
    char * dummy_name = NULL;
    flux_dev_t * dummy_dev = NULL;
    int serial_fd = -1;
    char * broker_url = DEFAULT_BROKER_URL;
    char * server_url = DEFAULT_SERVER_URL;

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
                case 's': arg_dest = &server_url; break;
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


    serial_fd = serial_open();
    if(serial_fd < 0) fprintf(stderr, "Unable to initialize serial port.\n");

    for(int i = 0; i < n_lux_ids; i++){
        snprintf(devices[i].name, 15, "lux:%08X", lux_ids[i]);
        devices[i].id = lux_ids[i];
        devices[i].bus_fd = -1;
        //devices[i].info = zhash_new();
    }

    if(flux_server_init(broker_url, server_url, 1000, verbose)) FAIL("Unable to intialize flux server.\n");

    if(dummy_name) dummy_dev = flux_dev_init(dummy_name, &dummy_request, NULL);
    if(serial_fd < 0 && !dummy_dev) FAIL("No devices to broadcast; exiting.\n");

    int rc = 0;
    while(!rc){
        if(serial_fd >= 0) enumerate_devices(serial_fd);
        for(int i = 0; i < 1000; i++){
            if(flux_server_poll() < 0){
                rc = -1;
                break;
            }
        }
    }

    // Teardown
    if(serial_fd) serial_close(serial_fd);

    flux_server_close();

    if(dummy_dev) flux_dev_del(dummy_dev);

    for(int i = 0; i < n_lux_ids; i++){
        //zhash_destroy(&devices[i].info);
    }
}
