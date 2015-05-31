#include "lib/err.h"

#include <flux.h>
#include <czmq.h>

#define DEFAULT_BROKER_URL "tcp://localhost:1365"

static flux_cli_t * client;
static int verbose = 0;

int main(int argc, char ** argv){
    char * broker_url = DEFAULT_BROKER_URL;

    argv++; argc--;
    if(argc) broker_url = *argv++, argc--;
    if(argc) verbose = streq(*argv++, "-v"), argc--;

    client = flux_cli_init(broker_url, verbose); 

    while(1){
        flux_id_t * ids;
        int n = flux_cli_id_list(client, "lux:", &ids);
        if(n >= 0){
            printf("\n%d Available devices:\n", n);
            for(int i = 0; i < n; i++){
                printf("    %.16s\n", ids[i]);

                zmsg_t * lmsg;
                lmsg = zmsg_new();
                zmsg_pushstr(lmsg, "Hello new protocol!");

                zmsg_t * reply = NULL;
                struct timespec tp;
                clock_gettime(CLOCK_REALTIME, &tp);
                long nsec = -tp.tv_nsec;
                int r = flux_cli_send(client, ids[i], "ECHO", &lmsg, &reply);
                clock_gettime(CLOCK_REALTIME, &tp);
                nsec += tp.tv_nsec;
                printf("echo response time: %ld\n", nsec);
                if(!r){
                    char * s = zframe_strdup(zmsg_first(reply));
                    printf("response: '%s'\n", s);
                    free(s);
                    zmsg_destroy(&reply);
                }else{
                    printf("send_lux error\n");
                    break;
                }
                zclock_sleep(100); 

                lmsg = zmsg_new();
                reply = NULL;
                r = flux_cli_send(client, ids[i], "INFO", &lmsg, &reply);
                if(!r){
                    zhash_t * info = zhash_unpack(zmsg_first(reply));
                    printf("info:\n");
                    zhash_save(info, "/dev/stdout");
                    printf("\n");
                }else{
                    printf("send_lux error\n");
                    break;
                }

                zmsg_destroy(&reply);
                zclock_sleep(300); 
            }
            free(ids);
        }else{
            printf("Error from broker\n");
            break;
        }
        zclock_sleep(1000);
    }

    flux_cli_del(client);
}
