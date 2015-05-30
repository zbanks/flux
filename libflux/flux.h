#ifndef __FLUX_H__
#define __FLUX_H__

#include <czmq.h>

typedef char flux_id_t[16];

// --- Server ---
struct _flux_dev;
typedef struct _flux_dev flux_dev_t;

typedef int (*request_fn_t)(void * args, const char * cmd, zmsg_t ** body, zmsg_t ** reply);

int flux_server_init(int verbose);
void flux_server_close();
int flux_server_poll();
void flux_server_set_poll_interval(int interval);

void flux_dev_del(flux_dev_t * device);
flux_dev_t * flux_dev_init(const char * broker, const flux_id_t name, request_fn_t request, void * args);

// --- Client ---
struct _flux_cli;
typedef struct _flux_cli flux_cli_t;

flux_cli_t * flux_cli_init(const char * broker, int verbose);
void flux_cli_del(flux_cli_t * client);
int flux_cli_send(flux_cli_t * client, const char * name, const char * cmd, zmsg_t ** body, zmsg_t ** reply);

int flux_cli_id_list(flux_cli_t * client, const char * prefix, flux_id_t ** ids);
int flux_cli_id_check(flux_cli_t * client, const char * prefix);

#endif
