#include "server/server.h"
#include "core/err.h"

#include <czmq.h>

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    server_init(); 

    zsock_t * writer = zsock_new_req("@" SERVER_ENDPOINT);
    if(!writer) FAIL("Unable to open ZMQ REQ socket\n");

    printf("Opened writer\n");

    if(zsock_send(writer, "s", "Hello, World!"))
        FAIL("Error sending data over writer\n");

    /*
    char * string;
    if(!zsock_recv(writer, "s", &string))
        FAIL("Error reading data over writer\n");
    printf("Main recieved '%s'\n", string);
    */
    zmsg_t * msg = zmsg_recv(writer); 
    if(!msg) FAIL("Writer recieved no response");

    char * string = zmsg_popstr(msg);
    if(!string) FAIL("No string in response");

    printf("Main recieved '%s'\n", string);

    zsock_destroy(&writer);
    if(writer) FAIL("Unable to destroy writer\n");

    server_del();
}
