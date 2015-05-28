//  mdwrkapi class - Majordomo Protocol Worker API
//  Implements the MDP/Worker spec at http://rfc.zeromq.org/spec:7.

#include "mdwrkapi.h"

//  Reliability parameters
#define HEARTBEAT_LIVENESS  3       //  3-5 is reasonable

//  .split worker class structure
//  This is the structure of a worker API instance. We use a pseudo-OO
//  approach in a lot of the C examples, as well as the CZMQ binding:

//  .split utility functions
//  We have two utility functions; to send a message to the broker and
//  to (re)connect to the broker:

//  Send message to broker
//  If no msg is provided, creates one internally

static void
s_mdwrk_send_to_broker (mdwrk_t *self, char *command, char *option,
                        zmsg_t *msg)
{
    msg = msg? zmsg_dup (msg): zmsg_new ();

    //  Stack protocol envelope to start of message
    if (option)
        zmsg_pushstr (msg, option);
    zmsg_pushstr (msg, command);
    zmsg_pushstr (msg, MDPW_WORKER);
    zmsg_pushstr (msg, "");

    if (self->verbose) {
        zclock_log ("I: sending %s to broker",
            mdps_commands [(int) *command]);
        zmsg_dump (msg);
    }
    zmsg_send (&msg, self->worker);
}

//  Connect or reconnect to broker

void s_mdwrk_connect_to_broker (mdwrk_t *self)
{
    if (self->worker)
        zsock_destroy (&self->worker);
    self->worker = zsock_new_dealer(self->broker);
    if (self->verbose)
        zclock_log ("I: connecting to broker at %s...", self->broker);

    //  Register service with broker
    s_mdwrk_send_to_broker (self, MDPW_READY, self->service, NULL);

    //  If dead_at is hit, queue is considered disconnected
    self->dead_at = zclock_time() + self->dead;
    self->heartbeat_at = zclock_time () + self->heartbeat;
}

//  .split constructor and destructor
//  Here we have the constructor and destructor for our mdwrk class:

//  Constructor

mdwrk_t *
mdwrk_new (char *broker,char *service, int verbose)
{
    assert (broker);
    assert (service);

    mdwrk_t *self = (mdwrk_t *) zmalloc (sizeof (mdwrk_t));
    self->broker = strdup (broker);
    self->service = strdup (service);
    self->verbose = verbose;
    self->dead = 10000;         //  msecs
    self->heartbeat = 2500;     //  msecs
    self->reconnect = 2500;     //  msecs

    s_mdwrk_connect_to_broker (self);
    return self;
}

//  Destructor

void
mdwrk_destroy (mdwrk_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        mdwrk_t *self = *self_p;
        zsock_destroy (&self->worker);
        free (self->broker);
        free (self->service);
        free (self);
        *self_p = NULL;
    }
}

//  .split configure worker
//  We provide two methods to configure the worker API. You can set the
//  heartbeat interval and retries to match the expected network performance.

zmsg_t * mdwrk_event(mdwrk_t * self){
    zmsg_t *msg = zmsg_recv (self->worker);
    if (!msg) return NULL;          //  Interrupted
    if (self->verbose) {
        zclock_log ("I: received message from broker:");
        zmsg_dump (msg);
    }
    self->dead_at = zclock_time() + self->dead;

    //  Don't try to handle errors, just assert noisily
    assert (zmsg_size (msg) >= 3);

    zframe_t *empty = zmsg_pop (msg);
    assert (zframe_streq (empty, ""));
    zframe_destroy (&empty);

    zframe_t *header = zmsg_pop (msg);
    assert (zframe_streq (header, MDPW_WORKER));
    zframe_destroy (&header);

    zframe_t *command = zmsg_pop (msg);
    if (zframe_streq (command, MDPW_REQUEST)) {
        //  We should pop and save as many addresses as there are
        //  up to a null part, but for now, just save one...
        self->reply_to = zmsg_unwrap (msg);
        zframe_destroy (&command);
        //  .split process message
        //  Here is where we actually have a message to process; we
        //  return it to the caller application:
        
        return msg;     //  We have a request to process
    }
    else
    if (zframe_streq (command, MDPW_HEARTBEAT))
        ;               //  Do nothing for heartbeats
    else
    if (zframe_streq (command, MDPW_DISCONNECT))
        s_mdwrk_connect_to_broker (self);
    else {
        zclock_log ("E: invalid input message");
        zmsg_dump (msg);
    }
    zframe_destroy (&command);
    zmsg_destroy (&msg);
    return NULL;
}

int mdwrk_check_heartbeat(mdwrk_t * self){
    if(zclock_time() > self->dead_at){
        if (self->verbose)
            zclock_log ("W: disconnected from broker - retrying...");
        zclock_sleep (self->reconnect);
        s_mdwrk_connect_to_broker (self);
        return -1;
    }
    //  Send HEARTBEAT if it's time
    if (zclock_time () > self->heartbeat_at) {
        s_mdwrk_send_to_broker (self, MDPW_HEARTBEAT, NULL, NULL);
        self->heartbeat_at = zclock_time () + self->heartbeat;
        return -1;
    }
    return 0;
}

//  Send reply, if any, to broker and wait for next request.

int mdwrk_send (mdwrk_t *self, zmsg_t **reply_p)
{
    //  Format and send the reply if we were provided one
    assert (reply_p);
    zmsg_t *reply = *reply_p;
    assert (reply || !self->expect_reply);
    if (reply) {
        assert (self->reply_to);
        zmsg_wrap (reply, self->reply_to);
        s_mdwrk_send_to_broker (self, MDPW_REPLY, NULL, reply);
        zmsg_destroy (reply_p);
    }
    self->expect_reply = 1;

    return 0;
}
