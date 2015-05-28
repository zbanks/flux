#ifndef __SERVER_SERVER_H__
#define __SERVER_SERVER_H__

#include <czmq.h>

struct resource;
typedef struct resource resource_t;

typedef zmsg_t * (*request_fn_t)(resource_t * resource, zmsg_t * msg, void * args);

#define N_MAX_RESOURCES 32

void server_init();
void server_del();
int server_run();
void server_set_poll_interval(int interval);
zsock_t * server_resource_get_socket(resource_t * resource);
void server_rm_resource(resource_t * resource);
struct resource * server_add_resource(char * broker, char * name, request_fn_t request, void * args, int verbose);

#endif
