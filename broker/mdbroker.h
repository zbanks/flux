#ifndef __BROKER_MDBROKER_H__
#define __BROKER_MDBROKER_H__
//  Majordomo Protocol broker
//  A minimal C implementation of the Majordomo Protocol as defined in
//  http://rfc.zeromq.org/spec:7 and http://rfc.zeromq.org/spec:8.

#include <czmq.h>
#include "mdp.h"

//  We'd normally pull these from config data

#define HEARTBEAT_LIVENESS  3       //  3-5 is reasonable
#define HEARTBEAT_INTERVAL  2500    //  msecs
#define HEARTBEAT_EXPIRY    HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS

//  .split broker class structure
//  The broker class defines a single broker instance:

typedef struct {
    zctx_t *ctx;                //  Our context
    void *socket;               //  Socket for clients & workers
    int verbose;                //  Print activity to stdout
    char *endpoint;             //  Broker binds to this endpoint
    zhash_t *services;          //  Hash of known services
    zhash_t *workers;           //  Hash of known workers
    zlist_t *waiting;           //  List of waiting workers
    int64_t heartbeat_at;       //  When to send HEARTBEAT
} broker_t;

broker_t * s_broker_new (int verbose);
void s_broker_destroy (broker_t **self_p);
void s_broker_bind (broker_t *self, char *endpoint);
void s_broker_worker_msg (broker_t *self, zframe_t *sender, zmsg_t *msg);
void s_broker_client_msg (broker_t *self, zframe_t *sender, zmsg_t *msg);
void s_broker_purge (broker_t *self);

//  .split service class structure
//  The service class defines a single service instance:

typedef struct {
    broker_t *broker;           //  Broker instance
    char *name;                 //  Service name
    zlist_t *requests;          //  List of client requests
    zlist_t *waiting;           //  List of waiting workers
    size_t workers;             //  How many workers we have
} service_t;

service_t * s_service_require (broker_t *self, zframe_t *service_frame);
void s_service_destroy (void *argument);
void s_service_dispatch (service_t *service, zmsg_t *msg);

//  .split worker class structure
//  The worker class defines a single worker, idle or active:

typedef struct {
    broker_t *broker;           //  Broker instance
    char *id_string;            //  Identity of worker as string
    zframe_t *identity;         //  Identity frame for routing
    service_t *service;         //  Owning service, if known
    int64_t expiry;             //  When worker expires, if no heartbeat
} worker_t;

worker_t * s_worker_require (broker_t *self, zframe_t *identity);
void s_worker_delete (worker_t *self, int disconnect);
void s_worker_destroy (void *argument);
void s_worker_send (worker_t *self, char *command, char *option, zmsg_t *msg);
void s_worker_waiting (worker_t *self);

//  .split broker constructor and destructor
//  Here are the constructor and destructor for the broker:


#endif
