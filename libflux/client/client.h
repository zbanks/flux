#ifndef __CLIENT_CLIENT_H__
#define __CLIENT_CLIENT_H__

#include <czmq.h>

struct client;
typedef struct client client_t;

client_t * client_init(char * broker, int verbose);
void client_del(client_t * client);
zmsg_t * client_send(client_t * client, char * name, zmsg_t ** msg);
#endif
