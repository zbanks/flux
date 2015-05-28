#include "core/err.h"
#include "client/client.h"
#include "lib/mdcliapi.h"

#include <czmq.h>

// Sends `n`@`frame` to address `lid` via the broker
// If `response` is not null, then the server needs to wait for a Lux response & return the data
// Return value is negative on error, 0 on success if `response` = 0
int send_lux(char * lid, char * frame, int n, unsigned char ** response){
    zmsg_t * lux_msg = zmsg_new();

    // First frame is a byte indicating if we want a response
    if(response)
        zmsg_addmem(lux_msg, "\001", 1);
    else
        zmsg_addmem(lux_msg, "\000", 1);

    // Second frame is the actual data
    zmsg_addmem(lux_msg, frame, n);

    // Send the data & get a response
    zmsg_t * lux_reply_msg = mdcli_send(client, lid, &lux_msg);

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

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    client_init(0); 

    while(1){
        zmsg_t * list_msg = zmsg_new();
        zmsg_pushstr(list_msg, "hi");
        zmsg_t * reply_msg = mdcli_send(client, "mmi.list", &list_msg);
        if(!reply_msg || zmsg_size(reply_msg) < 1) break;

        char * s;
        if(zframe_streq(zmsg_first(reply_msg), "200")){
            free(zmsg_popstr(reply_msg)); //pop 200
            printf("\nAvailable devices:\n");
            while((s = zmsg_popstr(reply_msg))){
                printf("    %s\n", s);

                unsigned char * response = 0;
                int n;
                n = send_lux(s, "lux msg!", 8, &response);
                if(n >= 0){
                    if(n > 0 && response){
                        printf("resp: %.*s\n", n, response);
                        free(response);
                    }
                }else{
                    printf("send_lux error %d\n", n);
                }
                free(s);
                zclock_sleep(300);
            }
        }else{
            printf("Error from broker\n");
        }
        
        zmsg_destroy(&reply_msg);
        zclock_sleep(1000);
    }

    client_del();
}
