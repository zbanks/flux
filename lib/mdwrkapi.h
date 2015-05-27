/*  =====================================================================
 *  mdwrkapi.h - Majordomo Protocol Worker API
 *  Implements the MDP/Worker spec at http://rfc.zeromq.org/spec:7.
 *  ===================================================================== */

#ifndef __MDWRKAPI_H_INCLUDED__
#define __MDWRKAPI_H_INCLUDED__

#include "czmq.h"
#include "mdp.h"

//  Structure of our class
typedef struct {
    zsock_t * worker;               //  Socket to broker
    char *broker;
    char *service;
    int verbose;                //  Print activity to stdout

    //  Heartbeat management
    int64_t heartbeat_at;       //  When to send HEARTBEAT
    int64_t dead_at;       //  When to send HEARTBEAT
    int dead;            //  How many attempts left
    int heartbeat;              //  Heartbeat delay, msecs
    int reconnect;              //  Reconnect delay, msecs

    int expect_reply;           //  Zero only at start
    zframe_t *reply_to;         //  Return identity, if any
} mdwrk_t;

mdwrk_t * mdwrk_new (char *broker,char *service, int verbose);
void mdwrk_destroy (mdwrk_t **self_p);
zmsg_t * mdwrk_event(mdwrk_t * self);
int mdwrk_send (mdwrk_t *self, zmsg_t **reply_p);
int mdwrk_check_heartbeat(mdwrk_t * self);

#endif
