#include "lib/err.h"

#include <flux.h>
#include <czmq.h>

#define BROKER_URL "tcp://localhost:5555"
static client_t * client;

// Sends `n`@`frame` to address `lid` via the broker
// If `response` is not null, then the server needs to wait for a Lux response & return the data
// Return value is negative on error, 0 on success if `response` = 0
/*
int send_lux(char * lid, char * frame, int n, unsigned char ** response){
    zmsg_t * lux_msg = zmsg_new();

    // Second frame is the actual data
    zmsg_addmem(lux_msg, frame, n);

    // Send the data & get a response
    zmsg_t * lux_reply_msg = client_send(client, lid, &lux_msg);

    // Parse response
    int rc;
    if(lux_reply_msg){
        zframe_t * req = zmsg_pop(lux_reply_msg);
        if(!req){
            rc = -1;
        }else if(zframe_size(req) == 0){ // Success!
            if(response){
                zframe_t * rep = zmsg_pop(lux_reply_msg);
                rc = zframe_size(rep);
                if(rc > 0){
                    *response = malloc(rc);
                    if(response)
                        memcpy(*response, zframe_data(rep), rc);
                    else
                        rc = -1;
                }
                zframe_destroy(&rep);
            }else{
                rc = 0;
            }
        }else{
            rc = - *(unsigned char *) zframe_data(req);
        }
        zframe_destroy(&req);
    }else{
        rc = -1;
    }

    zmsg_destroy(&lux_reply_msg);
    return rc;
}
*/
 
int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    client = client_init(BROKER_URL, 0); 

    while(1){
        flux_id_t * ids;
        int n = client_id_list(client, "lux:", &ids);
        if(n >= 0){
            printf("\n%d Available devices:\n", n);
            for(int i = 0; i < n; i++){
                printf("    %.16s\n", ids[i]);

                /*
                unsigned char * response = 0;
                int r;
                r = send_lux(ids[i], "lux msg!", 8, &response);
                if(r >= 0){
                    if(r > 0 && response){
                        printf("resp: %.*s\n", r, response);
                        free(response);
                    }
                }else{
                    printf("send_lux error %d\n", r);
                }
                */
                zmsg_t * lmsg = zmsg_new();
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
