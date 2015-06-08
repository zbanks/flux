#include "lib/err.h"

#include <flux.h>
#include <unistd.h> // sleep
#include <time.h>
#include <string.h> 
#include <nanomsg/nn.h> 

#define DEFAULT_BROKER_URL "tcp://localhost:1365"

static flux_cli_t * client;
static int verbose = 0;

int main(int argc, char ** argv){
    char * broker_url = DEFAULT_BROKER_URL;

    argv++; argc--;
    if(argc) broker_url = *argv++, argc--;
    if(argc) verbose = !strcmp(*argv++, "-v"), argc--;

    client = flux_cli_init(broker_url, verbose); 

    while(1){
        flux_id_t * ids = NULL;
        int n = flux_cli_id_list(client, &ids);
        if(n >= 0){
            printf("\n%d Available devices:\n", n);
            for(int i = 0; i < n; i++){
                printf("    %.16s\n", ids[i]);

                char * reply = NULL;
                struct timespec tp;
                clock_gettime(CLOCK_REALTIME, &tp);
                long nsec = -tp.tv_nsec;
                int r = flux_cli_send(client, ids[i], "ECHO", "Testing 123!", 16, &reply);
                clock_gettime(CLOCK_REALTIME, &tp);
                nsec += tp.tv_nsec;
                printf("echo response time: %ld nanoseconds\n", nsec);
                if(r && *reply){
                    printf("response: '%.*s'\n", r, reply);
                }else{
                    printf("send_lux error\n");
                    break;
                }
                sleep(1);

                reply = NULL;
                r = flux_cli_send(client, ids[i], "INFO", "", 0, &reply);
                if(r && reply){
                    //zhash_t * info = zhash_unpack(zmsg_first(reply));
                    printf("info:\n");
                    printf("%.*s", r, reply);
                    //zhash_save(info, "/dev/stdout");
                    printf("\n");
                }else{
                    printf("send_lux error\n");
                    break;
                }

                sleep(1); 
            }
            free(ids);
        }else{
            printf("Error from broker: %s\n", nn_strerror(errno));
            break;
        }
        sleep(1);
    }

    flux_cli_del(client);
}
