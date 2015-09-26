#include <errno.h>
#include <fcntl.h> 
#include <linux/serial.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "lib/err.h"
#include "lib/crc/crc.h"
#include "lib/serial/serial.h"

static int serial_set_attribs(int fd)
{
        struct termios tty;

        memset (&tty, 0, sizeof tty);
        
        tcgetattr(fd, &tty);
        //cfsetispeed(&tty, speed ? speed : 0010015);
        //cfsetospeed(&tty, speed ? speed : 0010015);

        //ioctl(3, SNDCTL_TMR_TIMEBASE or SNDRV_TIMER_IOCTL_NEXT_DEVICE or TCGETS, {c_iflags=0, c_oflags=0x4, c_cflags=0x1cbd, c_lflags=0, c_line=0, c_cc[VMIN]=1, c_cc[VTIME]=2, c_cc="\x03\x1c\x7f\x15\x04\x02\x01\x00\x11\x13\x1a\x00\x12\x0f\x17\x16\x00\x00\x00"}) = 0

        tty.c_cflag &= ~CSIZE;     // 8-bit chars
        tty.c_cflag |= CS8;     // 8-bit chars
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~CSTOPB; // 1 stop bit
        tty.c_cflag &= ~(PARENB|PARODD);
        tty.c_iflag &= ~(INPCK|ISTRIP);
        tty.c_iflag &= ~(IXON | IXOFF);
        tty.c_iflag |= IXANY;

        if (tcsetattr(fd, TCSANOW, &tty) != 0)
            return -1;

        return 0;
}

int serial_open()
{
    /* 
     * Initialize a new serial port (if an additional one exists)
     * Returns a ser_t instance if successful
     * Returns NULL if there are no available ports or there is an error
     */

    int fd;

    char dbuf[32];
    for(int i = 0; i < 9; i++){
        sprintf(dbuf, "/dev/ttyUSB%d", i);
        fd = open(dbuf, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
        if(fd >= 0){
            printf("Found output on '%s', %d\n", dbuf, port.fd);
            break;
        }

        sprintf(dbuf, "/dev/ttyACM%d", i);
        fd = open(dbuf, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
        if(fd >= 0){
            printf("Found output on '%s', %d\n", dbuf, port.fd);
            break;
        }
    }

    if(fd < 0) return -1;

    if(serial_set_attribs(fd) < 0) return -2;

    return fd;
}

void serial_close(int fd)
{
    close(fd);
}

static int cobs_encode(unsigned char* in_buf, int n, unsigned char* out_buf)
{
    int out_ptr = 0;
    unsigned char ctr = 1;
    for(int i = 0; i < n; i++)
    {
        if(in_buf[i] == 0)
        {
            out_buf[out_ptr] = ctr;
            out_ptr += ctr;
            ctr = 1;
        }
        else
        {
            out_buf[out_ptr + ctr] = in_buf[i];
            ctr++;
            if(ctr == 255)
            {
                out_buf[out_ptr] = ctr;
                out_ptr += ctr;
                ctr = 1;
            }
        }
    }
    out_buf[out_ptr] = ctr;
    out_ptr += ctr;
    return out_ptr; // success
}

static int cobs_decode(unsigned char* in_buf, int n, unsigned char* out_buf)
{
    int out_ptr = 0;
    unsigned char total = 255;
    unsigned char ctr = 255;
    for(int i = 0; i < n; i++)
    {
        if(in_buf[i] == 0) return -1; // invalid character

        if(ctr == total)
        {
            if(total < 255)
            {
                out_buf[out_ptr++] = 0;
            }
            total = in_buf[i];
            ctr = 1;
        }
        else
        {
            out_buf[out_ptr++] = in_buf[i];
            ctr++;
        }
    }

    if(out_buf[out_ptr] != 0) return -2; // generic decode error

    return out_ptr - 1; // success
}

static int frame(uint32_t destination, char* data, int len, char* result)
{
    static char* tmp[2048];
    int n;

    memcpy(&tmp[0], &destination, 4);
    memcpy(&tmp[4], data, len);
    crc_t crc = crc_init();
    crc = crc_update(crc, tmp, len + 4);
    crc = crc_finalize();
    memcpy(&tmp[len + 4], &crc, 4);
    n = cobs_encode(tmp, len + 8, result);
    if(n < 0) return -1; // encode error
    result[n] = 0;
    return n + 1; // success
}

static int unframe(char* packet, int n, uint32_t* destination, char* data)
{
    static char* tmp[2048];
    int len;

    len = cobs_decode(packet, n, tmp);
    if(len < 0) return -1; // decode error
    if(len < 8) return -2; // short packet
    
    crc_t crc = crc_init();
    crc = crc_update(crc, tmp, n);
    crc = crc_finalize();
    if(crc != 0x2144DF1C) return -3; // bad CRC

    memcpy(destination, &tmp[0], 4);
    memcpy(data, &tmp[4], len - 8);
    return len - 8; // success
}

static int lowlevel_read(int fd, char* data)
{
    static char* rx_buf[2048];
    int rx_ptr = 0;
    const struct timeval tv = {0, 10000};
    fd_set rfds;
    int n;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    for(;;)
    {
        r = select(1, &rfds, NULL, NULL, &tv);
        if(r < 0) return -1; // select error
        if(r == 0) return -2; // timeout
        n = read(fd, &rx_buf[rx_ptr], 2048 - rx_ptr);
        if(n < 0) return -3; // read error

        for(int i = 0; i < n; i++)
        {
            if(rx_buf[rx_ptr + i] == 0)
            {
                memcpy(data, rx_buf, rx_ptr + i - 1);
                return rx_ptr + i - 1;
            }
        }

        rx_ptr += n;
        if(rx_ptr == 2048) return -4; // buffer full
    }
}

static int lowlevel_write(int fd, char* data, int n)
{
    static char* tx_buf[2048];
    int tx_ptr = 0;
    const struct timeval tv = {0, 10000};
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    memcpy(tx_buf, data, n);
    tx_buf[n] = 0;

    for(;;)
    {
        r = select(1, NULL, &wfds, NULL, &tv);
        if(r < 0) return -1; // select error
        if(r == 0) return -2; // timeout
        n_written = write(fd, &tx_buf[tx_ptr], n + 1 - tx_ptr);
        if(n_written < 0) return -3; // read error
        tx_ptr += n_written;
        if(tx_ptr == n + 1) return 0; // Success
    }
}

static int clear_rx(int fd)
{
    static char* rx_buf[2048];
    int n;
    do
    {
        n = read(fd, rx_buf, 2048);
        if(n == EAGAIN) return 0; // Nothing to read, success
        if(n < 0) return -1; // Read error
    } while(n);

    return 0; // Successfully flushed
}

int write(int fd, uint32_t destination, char* data, int len, int retry)
{
    return -1; // Not implemented
}

int command(int fd, uint32_t destination, char* data, int len, char* response, int retry)
{
    return -1; // Not implemented
}

