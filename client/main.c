#include "core/err.h"
#include "client/client.h"
#include "lib/mdcliapi.h"

#include <czmq.h>

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    client_init(0); 

    while(1){
        zmsg_t * list_msg = zmsg_new();
        zmsg_pushstr(list_msg, "hi");
        zmsg_t * reply_msg = mdcli_send(client, "mmi.list", &list_msg);
        if(!reply_msg) break;

        char * s;
        while((s = zmsg_popstr(reply_msg))) printf(">%s\n", s);
        
        zmsg_destroy(&reply_msg);
        zclock_sleep(1000);
    }

    client_del();
}
