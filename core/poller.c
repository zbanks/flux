#include <czmq.h>

#include "core/err.h"
#include "core/poller.h"
#include "server/server.h"
#include "test/client.h"

#define SOCKS X(server) X(client)

void poller_run(){
    #define X(s) s,
    zpoller_t * poller = zpoller_new(SOCKS 0);
    #undef X
    if(!poller) FAIL("Unable to set up reader poller\n");

    while(1){
        zsock_t * which = (zsock_t *) zpoller_wait(poller, POLL_TIMEOUT_MSEC);
        if(!which){
            if(zpoller_expired(poller)) continue;
            else break;
        }

        if(0){}
            #define X(s) else if(which == s){ s ##_event(); }
            SOCKS
            #undef X
        else
            FAIL("Invalid socket returned from poller\n");
    }

    zpoller_destroy(&poller);
    if(poller) FAIL("Unable to free poller\n");
}
