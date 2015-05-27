#include "core/err.h"
#include "client/client.h"

#include <czmq.h>

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    client_init(); 
    //while(!client_event());
    
    if(zsock_send(client, "s", "Hello, world!")) FAIL("Unable to send message");

    client_event();

    client_del();
}
