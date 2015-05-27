#ifndef __SERVER_SERVER_H__
#define __SERVER_SERVER_H__

#include <czmq.h>
#include "lib/mdwrkapi.h"

struct resource;
struct resource {
    mdwrk_t * worker;
    zsock_t * socket;
    char * name;
    int id;
    int verbose;
    zmsg_t * (*request)(struct resource * resource, zmsg_t * msg);
};

#define N_MAX_RESOURCES 16

void server_init();
void server_del();
void server_run();
struct resource * server_add_lux_resource(int id, int verbose);
void server_del_resource(struct resource * resource);

#endif
