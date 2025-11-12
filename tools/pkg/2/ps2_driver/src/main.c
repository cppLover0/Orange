
#include <orange/dev.h>
#include <orange/io.h>
#include <orange/log.h>

#include <stdio.h>
#include <fcntl.h>
#include <poll.h>

#include <stdlib.h>

#include <stdint.h>

#include <string.h>

#include <unistd.h>

#define STATUS 0x64
#define COMMAND 0x64
#define DATA 0x60
#define RESEND 0xFE
#define ACK 0xFA
#define ECHO 0xEE

static inline char ps2_isfull() {
	return (inb(0x64) & 2) == 2;
}

static inline char ps2_wait() {
    return (inb(0x64) & 1) == 0;
}

void ps2_writecmd(uint8_t cmd) {
    while(ps2_isfull());
    outb(0x64,cmd);
}

void ps2_writedata(uint8_t cmd) {
    while(ps2_isfull());
    outb(0x60,cmd);
}

void ps2_flush() {
    int timeout = 1000;
    while((inb(STATUS) & 1) && --timeout > 0) inb(DATA);
}

uint8_t ps2_read() {
    while(ps2_wait());
    return inb(0x60);
}

uint8_t ps2_readtimeout() {
    int timeout = 100;
    while(ps2_wait() && --timeout > 0) usleep(100);
    if(timeout <= 0)
        return 0;
    return inb(0x60);
}

uint8_t ps2_readlongtimeout() {
    int timeout = 100;
    while(ps2_wait() && --timeout > 0) usleep(1000);
    if(timeout <= 0)
        return 0;
    return inb(0x60);
}



void ps2_writeport(uint8_t port, uint8_t cmd) {
    if(port == 2)
        ps2_writecmd(0xD4);
    
    ps2_writedata(cmd);
}

int dual_channel = 0;

void ps2_disableport(uint8_t port) {
    ps2_writeport(port,0xF5);
    int retry = 0;
    while(1) {
        uint8_t response = ps2_readtimeout();
        if(!response)
            break;
        else if(response == RESEND) {
            if(++retry == 10)
                break;
            ps2_writeport(port,0xF5);
        }
    }
    ps2_flush();
}

void ps2_enableport(uint8_t port) {
    ps2_writeport(port,0xF4);
    int retry = 0;
    while(1) {
        uint8_t response = ps2_readtimeout();
        if(!response)
            break;
        else if(response == ACK)
            break;
        else if(response == RESEND) {
            if(++retry == 10)
                break;
            ps2_writeport(port,0xF4);
        }
    }
    ps2_flush();
}

char ps2_selftest(uint8_t port) {
    ps2_disableport(port);
    ps2_writeport(port,0xFF);
    int retry = 0;
    while(1) {
        uint8_t response = ps2_readtimeout();
        if(!response)
            break;
        else if(response == RESEND) {
            if(++retry == 10)
                break;
            ps2_writeport(port,0xFF);
        } else if(response == ACK) 
            break;
    }

    while(1) {
        uint8_t response = ps2_readlongtimeout();
        if(!response)
            break;
        else if(response == 0xAA) 
            break;
        else if(response == 0xFC) {
            log(LEVEL_MESSAGE_FAIL,"Failed to test PS/2 port %d\n",port);
            return 0;
        }
    }

    // used from astral src 
    while(1) {
        if(!ps2_readtimeout())
            break;
    }

    ps2_flush();
    return 1;
}

#define MOUSE_LB (1 << 0)
#define MOUSE_RB (1 << 1)
#define MOUSE_MB (1 << 2)
#define MOUSE_B4 (1 << 3)
#define MOUSE_B5 (1 << 4)

typedef struct {
    unsigned char buttons;
    unsigned char x;
    unsigned char y;
    unsigned char z;
} __attribute__((packed)) mouse_packet_t;

int main() {

    int pid = fork();

    if(pid > 0)
        exit(0);

    int masterinput = open("/dev/masterps2keyboard",O_RDWR);
    int mastermouse = open("/dev/mastermouse",O_RDWR);

    if(mastermouse < 0) {
        perror("Can't open /dev/mastermouse device");
        exit(-1);
    }

    if(masterinput < 0) {
        perror("Can't open /dev/masterkeyboard device");
        exit(-1);
    }
    
    liborange_setup_iopl_3();

    int pic = open("/dev/pic",O_RDWR);

    if(pic < 0) {
        perror("Can't open /dev/pic device");
        exit(-1);
    }

    ps2_writecmd(0xAD);
    ps2_writecmd(0xA7);

    ps2_writecmd(0x20);
    uint8_t config = ps2_read();
    config |= (1 << 0) | (1 << 1) | (1 << 6);
    ps2_writecmd(0x60);
    ps2_writedata(config);

    ps2_enableport(1);
    ps2_enableport(2);

    // dont need identify just know what port 1 is kbd port 2 is mouse 

    liborange_pic_create_t irq_create_request;
    irq_create_request.flags = 0;

    ps2_writecmd(0xAE);

    irq_create_request.irq = 1;
    write(pic,&irq_create_request,sizeof(liborange_pic_create_t));
    
    ps2_writecmd(0xA8);

    irq_create_request.irq = 12;
    write(pic,&irq_create_request,sizeof(liborange_pic_create_t));
        
    ps2_flush();

    int irq1 = open("/dev/irq1",O_RDWR);
    int irq12 = open("/dev/irq12",O_RDWR);

    struct pollfd irq_fds[2];

    irq_fds[0].events = POLLIN;
    irq_fds[0].fd = irq1;
    irq_fds[1].events = POLLIN;
    irq_fds[1].fd = irq12;

    int mouse_seq = 0;

    
    if(1) {

        uint8_t mouse_buffer[4] = {0,0,0,0};

        while(1) {
            int num_events = poll(irq_fds,2,200);
            if(num_events < 0) {
                perror("Failed to poll irq fds");
                exit(-1);
            }

            if(num_events == 0) {
                mouse_seq = 0;
                ps2_flush();
                int val = 1;
                write(irq1,&val,1);
                write(irq12,&val,1);
            }

            if(irq_fds[1].revents & POLLIN) {
                char val = 0;
                int count = read(irq12,&val,1);
                if(count && val == 1) {
                    
                    mouse_buffer[mouse_seq++] = inb(DATA); 
                    
                    if(mouse_seq != 3) { 
                        write(irq1,&val,1);
                        write(irq12,&val,1);
                        continue; }

                    mouse_packet_t packet = {0,0,0,0};

                    packet.x = mouse_buffer[1] - (mouse_buffer[0] & 0x10 ? 0x100 : 0);
                    packet.y = mouse_buffer[2] - (mouse_buffer[0] & 0x20 ? 0x100 : 0);
                    packet.z = (mouse_buffer[3] & 0x7) * (mouse_buffer[3] & 0x8 ? -1 : 1);
                    
                    packet.buttons |= (mouse_buffer[0] & 1) ? MOUSE_LB : 0;
                    packet.buttons |= (mouse_buffer[0] & 2) ? MOUSE_RB : 0;
                    packet.buttons |= (mouse_buffer[0] & 4) ? MOUSE_MB : 0;

                    packet.buttons |= (mouse_buffer[0] & 0x10) ? MOUSE_B4 : 0;
                    packet.buttons |= (mouse_buffer[0] & 0x20) ? MOUSE_B5 : 0; 

                    write(mastermouse,&packet,4);

                    mouse_seq = 0;
                    
                    write(irq1,&val,1); 
                    write(irq12,&val,1);
                    
                }
            }

            if(irq_fds[0].revents & POLLIN) { 
                char val = 0;
                int count = read(irq1,&val,1);
                if(count && val == 1) {
                    uint8_t scancodes[64];
                    int i = 0;
                    memset(scancodes,0, 64);
                    while((inb(0x64) & 1)) { 
                        uint8_t scancode = inb(0x60);
                        if(i < 64)
                            scancodes[i++] = scancode;
                    }
                    write(masterinput,scancodes,i);
                    write(irq1,&val,1); /* Ask kernel to unmask this irq */
                    write(irq12,&val,1); /* irq 12 and irq 1 are connected */
                }
            }

        }
    }
    exit(0);

}