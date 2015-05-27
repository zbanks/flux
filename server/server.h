#ifndef __SERVER_SERVER_H__
#define __SERVER_SERVER_H__

#define SERVER_ENDPOINT "tcp://127.0.0.1:5568"
#define SERVER_POLL_TIMEOUT_MSEC 500

void server_init();
void server_del();

#endif
