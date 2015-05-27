#include "core/err.h"
#include "server/server.h"

#include <czmq.h>

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    server_init(); 
    while(!server_event());
    
    /*
    zpoller_t * poller = zpoller_new(server);
    if(!poller) FAIL("Unable to set up reader poller\n");

    while(1){
        zsock_t * which = (zsock_t *) zpoller_wait(poller, POLL_TIMEOUT_MSEC);
        if(!which){
            if(zpoller_expired(poller)) continue;
            else break;
        }
        if(which != server) FAIL("Invalid socket returned from poller\n");
        server_event();
    }

    zpoller_destroy(&poller);
    if(poller) FAIL("Unable to free poller\n");
    */

    server_del();
}
