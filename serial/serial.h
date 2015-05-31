#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <termios.h>
#include "lux_hal.h"
#include "lux_wire.h"

typedef struct ser {
    int fd;
    int write_only;
} ser_t;

ser_t * serial_init(int write_only);
void serial_close(ser_t * s);

struct lux_frame {
    uint32_t destination;
    int length;
    union lux_command_frame data;
};

char lux_tx_packet(ser_t * s, struct lux_frame *);
char lux_rx_packet(ser_t * s, struct lux_frame *, int timeout_ms);
char lux_command_ack(ser_t * s, struct lux_frame *cmd, int timeout_ms);
char lux_command_response(ser_t * s, struct lux_frame *cmd, struct lux_frame *response, int timeout_ms);

#endif
