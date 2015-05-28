#include "core/err.h"
#include "server/server.h"

#include <czmq.h>

int main(int argc, char ** argv){
    UNUSED(argc);
    UNUSED(argv);

    server_add_lux_resource(0x00000001, 0);

    server_init();

    server_add_lux_resource(0x00000002, 0);

    server_run(); 

    server_del();
}
