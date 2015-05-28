#ifndef __SERVER_SERVER_H__
#define __SERVER_SERVER_H__

#include <czmq.h>
#include "lib/mdwrkapi.h"


struct resource;

typedef zmsg_t * (*request_fn_t)(struct resource * resource, zmsg_t * msg, void * args);

struct resource {
    mdwrk_t * worker;
    zsock_t * socket;
    char * name;
    void * args;
    int verbose;
    request_fn_t request;
};

#define N_MAX_RESOURCES 16

void server_init();
void server_del();
int server_run();
void server_rm_resource(struct resource * resource);
struct resource * server_add_resource(char * name, request_fn_t request, void * args, int verbose);

#endif
