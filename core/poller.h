#ifndef __CORE_POLLER_H__
#define __CORE_POLLER_H__

#include <czmq.h>

#define POLL_TIMEOUT_MSEC 500

void poller_run();

#endif
