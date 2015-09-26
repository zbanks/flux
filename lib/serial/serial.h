#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdint.h>

int serial_open();
void serial_close(int fd);

int lux_write(int fd, uint32_t destination, char* data, int len);
int lux_command(int fd, uint32_t destination, char* data, int len, char* response, int retry);

#endif
