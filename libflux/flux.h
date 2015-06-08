#ifndef __FLUX_H__
#define __FLUX_H__

#include <stddef.h>

typedef char flux_id_t[16];
typedef char flux_cmd_t[16];

// --- Server ---
struct _flux_dev;
typedef struct _flux_dev flux_dev_t;

typedef int (*request_fn_t)(void * args, const flux_cmd_t cmd, char * body, size_t body_size, char ** reply); 


int flux_server_init(const char * broker_url, const char * rep_url, int verbose);
void flux_server_close();
int flux_server_poll();
void flux_server_set_poll_interval(int interval);

void flux_dev_del(flux_dev_t * device);
flux_dev_t * flux_dev_init(const flux_id_t name, request_fn_t request, void * args);

// --- Client ---
struct _flux_cli;
typedef struct _flux_cli flux_cli_t;

flux_cli_t * flux_cli_init(const char * broker_url, int verbose);
void flux_cli_del(flux_cli_t * client);
int flux_cli_send(flux_cli_t * client, const flux_id_t dest, const flux_cmd_t cmd, const char * body, size_t body_size, char ** reply); // Free reply afterwards

int flux_cli_id_list(flux_cli_t * client, flux_id_t ** ids); // Remember to free ids afterwards
//int flux_cli_id_check(flux_cli_t * client, const char * prefix);

#endif
