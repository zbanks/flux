#include "lib/err.h"

#include <flux.h>
#include <czmq.h>

#define BROKER_URL "tcp://localhost:5555"

static client_t * client;

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    int verbose = 0;

    client = client_init(BROKER_URL, verbose); 

    while(1){
        flux_id_t * ids;
        int n = client_id_list(client, "lux:", &ids);
        if(n >= 0){
            printf("\n%d Available devices:\n", n);
            for(int i = 0; i < n; i++){
                printf("    %.16s\n", ids[i]);

                zmsg_t * lmsg;
                lmsg = zmsg_new();
                zmsg_pushstr(lmsg, "Hello new protocol!");

                zmsg_t * reply = NULL;
                int r = client_send(client, ids[i], "ECHO", &lmsg, &reply);
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
                r = client_send(client, ids[i], "INFO", &lmsg, &reply);
                if(!r){
                    zhash_t * info = zhash_unpack(zmsg_first(reply));
                    printf("info:");
                    zhash_save(info, "/dev/stdout");
                    printf("\n");

                    zmsg_destroy(&reply);
                }else{
                    printf("send_lux error\n");
                    break;
                }

                zclock_sleep(300); 
            }
            free(ids);
        }else{
            printf("Error from broker\n");
            break;
        }
        printf("lux:00000001 -> %d\n", client_id_check(client, "lux:00000001"));
        printf("lux:00000abc -> %d\n", client_id_check(client, "lux:00000abc"));
        zclock_sleep(1000);
    }

    client_del(client);
}
