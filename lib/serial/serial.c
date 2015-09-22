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
#include "lib/lux/inc/lux.h"
#include "lib/serial/serial.h"

static char lux_is_transmitting;
static crc_t crc;
int serial_set_attribs (int, int);

// Currently, there is only 1 port
// This will be expanded
static ser_t port = {
    .fd = -1,
    .write_only = 0,
};

uint8_t match_destination(uint8_t* dest){
    uint32_t addr = *(uint32_t *)dest;
    return addr == 0;
}

void rx_packet() {
}

ser_t * serial_init(int write_only){
    /* 
     * Initialize a new serial port (if an additional one exists)
     * Returns a ser_t instance if successful
     * Returns NULL if there are no available ports or there is an error
     */
    if(port.fd >= 0) return NULL;

    char dbuf[32];
    for(int i = 0; i < 9; i++){
        sprintf(dbuf, "/dev/ttyUSB%d", i);
        port.fd = open(dbuf, O_RDWR | O_NOCTTY | O_SYNC);
        if(port.fd >= 0){
            printf("Found output on '%s', %d\n", dbuf, port.fd);
            break;
        }

        sprintf(dbuf, "/dev/ttyACM%d", i);
        port.fd = open(dbuf, O_RDWR | O_NOCTTY | O_SYNC);
        if(port.fd >= 0){
            printf("Found output on '%s', %d\n", dbuf, port.fd);
            break;
        }
    }
    if(port.fd < 0) return NULL;

    port.idx = 0;
    port.write_only = write_only;
    if(write_only) printf("Opened serial in write_only mode.\n");

    lux_fn_match_destination = &match_destination;
    lux_fn_rx = &rx_packet;
    lux_init();

    return &port;
}

void serial_close(ser_t * s){
    if(s){
        if(s->fd >= 0) close(s->fd);
        s->fd = -1;
    }
}

char lux_command_response(ser_t * s, struct lux_frame *cmd, struct lux_frame *response, int timeout_ms){
    /* 
     * Transmits command `*cmd`, and waits for response and saves it to `*response`
     * Returns 0 on success 
     */
    char r;
    //TODO:port
    if(s->write_only){
        fprintf(stderr, "Attempted to read response on write_only port.\n");
        return -50;
    }
    
    if((r = lux_tx_packet(s, cmd)))
        return r;

    if(response){
        if((r = lux_rx_packet(s, response, timeout_ms)))
            return r;

        if(response->destination != 0)
            return -10;
    }
    return 0;
}

char lux_command_ack(ser_t * s, struct lux_frame *cmd, int timeout_ms){
    /*
     * Transmits command `*cmd`, and waits for ack response
     * Returns 0 on success
     */
    char r;
    struct lux_frame response;
    //TODO
    if(s->write_only){
        fprintf(stderr, "Attempted to read response on write_only port.\n");
        return -50;
    }

    if((r = lux_command_response(s, cmd, &response, timeout_ms)))
        return r;

    if(response.destination != 0)
        return -20;

    if(response.length != 1)
        return -21;

    return response.data.raw[0];
}

char lux_tx_packet(ser_t * s, struct lux_frame *cmd){
    /*
     * Transmits command `*cmd`
     * Returns 0 on success
     */
    //TODO
    UNUSED(s);
    lux_hal_disable_rx();

    if(!cmd)
        return -30;

    memcpy(lux_destination, &cmd->destination, sizeof(cmd->destination));
    if(cmd->length > LUX_PACKET_MAX_SIZE)
        return -31;
    lux_packet_length = cmd->length;
    memcpy(lux_packet, &cmd->data, cmd->length);

    lux_packet_in_memory = 0;
    lux_start_tx();
    while(lux_is_transmitting) lux_codec();
    return 0;
}

char lux_rx_packet(ser_t * s, struct lux_frame *response, int timeout_ms){
    /* 
     * Attempts to recieve a packet (with a timeout)
     * Puts recieved packet into `*response`
     * Returns 0 on success
     */
    //TODO
    if(s->write_only){
        fprintf(stderr, "Attempted to read response on write_only port.\n");
        return -50;
    }

    for(int j = 0; j <= timeout_ms; j++){
        for(int i = 0; i < 1100; i++)
            lux_codec();   
        if(lux_packet_in_memory){
            break;
        }
    }

    if(!lux_packet_in_memory)
        return -40;

    lux_packet_in_memory = 0;

    if(!response)
        return -41;

    response->length = lux_packet_length;
    memcpy(&response->destination, lux_destination, sizeof(response->destination));
    memset(&response->data, 0, LUX_PACKET_MAX_SIZE);
    memcpy(&response->data, lux_packet, lux_packet_length);

    return 0;
}

void lux_hal_enable_rx(){
    lux_is_transmitting = 0;
    /*
    //TODO:port
    if(!port.write_only){
        const int r = TIOCM_RTS;
        usleep(2000);
        ioctl(port.fd, TIOCMBIS, &r);
    }
    */
}

void lux_hal_disable_rx(){
    lux_is_transmitting = 1;
    /*
    const int r = TIOCM_RTS;
    ioctl(port.fd, TIOCMBIC, &r);
    //TODO:port
    if(!port.write_only){
        usleep(2000);
    }
    */
}

void lux_hal_enable_tx(){}
void lux_hal_disable_tx(){}

int16_t lux_hal_bytes_to_read(){
    int bytes_avail;
    //TODO:port
    ioctl(port.fd, FIONREAD, &bytes_avail);
    return bytes_avail;
}

uint8_t lux_hal_read_byte(){
    uint8_t byte = 0;
    //TODO:port
    if(!read(port.fd, &byte, 1))
        printf("Error writing byte to serial port\n");
    return byte;
}

//TODO: clean up 
int16_t lux_hal_bytes_to_write(){
    return sizeof(port.buffer) - port.idx;
}

void lux_hal_write_byte(uint8_t byte){
    //TODO:port
    port.buffer[port.idx++] = byte;
    if (port.idx == sizeof(port.buffer))
        lux_hal_tx_flush(); // this might not work
    /*
    if(!write(port.fd, &byte, 1))
        printf("Error writing byte to serial port\n");
        */
}

uint8_t lux_hal_tx_flush(){
    //TODO:port
    //tcflush(port.fd, TCOFLUSH);
    if(!write(port.fd, port.buffer, port.idx))
        printf("Error writing byte to serial port\n");
    port.idx = 0;
    return 1;
}

void lux_hal_reset_crc(){
    crc = crc_init();
}

void lux_hal_crc(uint8_t byte){
    crc = crc_update(crc, &byte, 1);
}

uint8_t lux_hal_crc_ok(){
    return crc_finalize(crc) == 0x2144DF1C; 
}

void lux_hal_write_crc(uint8_t* ptr){
    crc = crc_finalize(crc);
    memcpy(ptr, &crc, 4);
}

int serial_set_attribs (int fd, int speed)
{
        struct termios tty;

        memset (&tty, 0, sizeof tty);
        
        tcgetattr(fd, &tty);
        cfsetispeed(&tty, speed ? speed : 0010015);
        cfsetospeed(&tty, speed ? speed : 0010015);

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

        lux_hal_disable_rx();

        return 0;
}
