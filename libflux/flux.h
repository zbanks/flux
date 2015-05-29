#ifndef __FLUX_H__
#define __FLUX_H__

#include <czmq.h>

typedef char flux_id_t[16];

// --- Server ---
struct resource;
typedef struct resource resource_t;

typedef int (*request_fn_t)(void * args, const char * cmd, zmsg_t * msg, zmsg_t ** reply);

void server_init();
void server_del();
int server_run();
void server_set_poll_interval(int interval);
zsock_t * server_resource_get_socket(resource_t * resource);
void server_rm_resource(resource_t * resource);
struct resource * server_add_resource(char * broker, flux_id_t name, request_fn_t request, void * args, int verbose);

// --- Client ---

struct client;
typedef struct client client_t;

int client_id_list(client_t * client, char * prefix, flux_id_t ** ids);
int client_id_check(client_t * client, char * prefix);

client_t * client_init(char * broker, int verbose);
void client_del(client_t * client);
int client_send(client_t * client, char * name, char * cmd, zmsg_t ** msg, zmsg_t ** reply);

#endif
