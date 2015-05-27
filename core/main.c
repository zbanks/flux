#include "core/err.h"
#include "core/poller.h"
#include "server/server.h"
#include "test/client.h"

#include <czmq.h>

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    server_init(); 
    client_init();
    
    if(zsock_send(client, "s", "Hello, World!"))
        FAIL("Error sending data over writer\n");

    poller_run();

    client_del();
    server_del();
}
