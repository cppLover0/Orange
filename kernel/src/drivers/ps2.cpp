#include <drivers/tsc.hpp>
#include <generic/mm/paging.hpp>
#include <generic/mm/pmm.hpp>
#include <generic/mm/heap.hpp>
#include <generic/vfs/vfs.hpp>
#include <etc/etc.hpp>
#include <etc/logging.hpp>

#include <drivers/pci.hpp>
#include <generic/vfs/evdev.hpp>

#include <arch/x86_64/scheduling.hpp>
#include <drivers/ps2.hpp>

#include <etc/assembly.hpp>
#include <drivers/io.hpp>

void ps2_wait_write() {
    drivers::io io;
    while (io.inb(PS2_STATUS) & (1 << 1));
}

void ps2_wait_read() {
    drivers::io io;
    while (!(io.inb(PS2_STATUS) & (1 << 0)));
}

void ps2_writecmddata(char cmd, char data) {
    drivers::io io;
    ps2_wait_write();
    io.wait();
    io.outb(PS2_STATUS, cmd);
    io.wait();
    io.outb(PS2_DATA, data);
}

void ps2_writecmd(char cmd) {
    drivers::io io;
    ps2_wait_write();
    io.outb(PS2_STATUS, cmd);
}

void ps2_writedata(char cmd) {
    drivers::io io;
    ps2_wait_write();
    io.outb(PS2_DATA, cmd);
}

char ps2_not_empty() {
    drivers::io io;
    return (io.inb(PS2_STATUS) & 1) == 0;
}

int ps2_is_timeout = 0;

char ps2_read_data() {
    drivers::io io;
    int timeout = 0;
    while(ps2_not_empty()) {
        if(timeout++ == 100) {
            ps2_is_timeout = 1;
            return 0;
        }
        drivers::tsc::sleep(100);
    }
    return io.inb(PS2_DATA);
}

void ps2_flush() {
    drivers::io io;
    while(io.inb(PS2_STATUS) & (1 << 0)) {
        io.inb(PS2_DATA);
    }
}

#define STATUS 0x64
#define COMMAND 0x64
#define DATA 0x60
#define RESEND 0xFE
#define ACK 0xFA
#define ECHO 0xEE

void ps2_writeport(uint8_t port, uint8_t cmd) {
    if(port == 2)
        ps2_writecmd(0xD4);
    
    ps2_writedata(cmd);
}

static inline char ps2_wait() {
    drivers::io io;
    return (io.inb(0x64) & 1) == 0;
}

uint8_t ps2_readtimeout() {
    drivers::io io;
    int timeout = 100;
    while(ps2_wait() && --timeout > 0) drivers::tsc::sleep(100);
    if(timeout <= 0)
        return 0;
    return io.inb(0x60);
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

#define MOUSE_LB (1 << 0)
#define MOUSE_RB (1 << 1)
#define MOUSE_MB (1 << 2)
#define MOUSE_B4 (1 << 3)
#define MOUSE_B5 (1 << 4)

/*
input0_fd = open("/dev/masterps2keyboard",O_RDWR);
    mouse_fd = open("/dev/mastermouse",O_RDWR);
*/

#define REL_X			0x00
#define REL_Y			0x01
#define REL_Z			0x02
#define REL_RX			0x03
#define REL_RY			0x04
#define REL_RZ			0x05
#define REL_HWHEEL		0x06
#define REL_DIAL		0x07
#define REL_WHEEL		0x08
#define REL_MISC		0x09

typedef struct {
    unsigned char buttons;
    unsigned char x;
    unsigned char y;
    unsigned char z;
} __attribute__((packed)) mouse_packet_t;

int mouse_evdev = 0;
int kbd_evdev = 0;

mouse_packet_t last_pack = {0};

void ps2_process(void* arg) {
    int mouse_seq = 0;
    drivers::io io;
    if(1) {

        uint8_t mouse_buffer[4] = {0,0,0,0};

        while(1) {

            if(1) {

                int val = 1;

                uint8_t status = io.inb(0x64);
                while(status & 1) { 
                    int data = io.inb(DATA);

                    if(status & (1 << 5)) {
                        mouse_buffer[mouse_seq++] = data;
                        
                        if(mouse_seq == 3) { 
                            mouse_packet_t packet = {0,0,0,0};

                            packet.x = mouse_buffer[1] - (mouse_buffer[0] & 0x10 ? 0x100 : 0);
                            packet.y = mouse_buffer[2] - (mouse_buffer[0] & 0x20 ? 0x100 : 0);
                            packet.z = (mouse_buffer[3] & 0x7) * (mouse_buffer[3] & 0x8 ? -1 : 1);
                            
                            packet.buttons |= (mouse_buffer[0] & 1) ? MOUSE_LB : 0;
                            packet.buttons |= (mouse_buffer[0] & 2) ? MOUSE_RB : 0;
                            packet.buttons |= (mouse_buffer[0] & 4) ? MOUSE_MB : 0;

                            packet.buttons |= (mouse_buffer[0] & 0x10) ? MOUSE_B4 : 0;
                            packet.buttons |= (mouse_buffer[0] & 0x20) ? MOUSE_B5 : 0; 

                            userspace_fd_t fd;
                            memset(&fd,0,sizeof(userspace_fd_t));
                            memcpy(fd.path,"/dev/mastermouse",strlen("/dev/mastermouse"));
                            fd.is_cached_path = 1;

                            asm volatile("cli");
                            std::uint64_t current_nano = drivers::tsc::currentnano();
                            input_event ev;
                            ev.time.tv_sec = current_nano / 1000000000;
                            ev.time.tv_usec = (current_nano & 1000000000) / 1000;
                            if(packet.x) {
                                ev.code = REL_X;
                                ev.type = 2;
                                ev.value = (packet.x & 0x80) ? (packet.x | 0xFFFFFF00) : packet.x;
                                vfs::evdev::submit(mouse_evdev,ev);
                            }

                            if(packet.y) {
                                ev.code = REL_Y;
                                ev.type = 2;
                                ev.value = (packet.y & 0x80) ? (packet.y | 0xFFFFFF00) : packet.y;
                                vfs::evdev::submit(mouse_evdev,ev);
                            }

                            if(packet.z) {
                                ev.code = REL_Z;
                                ev.type = 2;
                                ev.value = (packet.z & 0x80) ? (packet.z | 0xFFFFFF00) : packet.z;
                                vfs::evdev::submit(mouse_evdev,ev);
                            }

                            if((packet.buttons & MOUSE_LB) && !(last_pack.buttons & MOUSE_LB)) {
                                ev.code = 272;
                                ev.type = 1;
                                ev.value = 1;
                                vfs::evdev::submit(mouse_evdev,ev);
                            } else if(!(packet.buttons & MOUSE_LB) && (last_pack.buttons & MOUSE_LB)) {
                                ev.code = 272;
                                ev.type = 1;
                                ev.value = 0;
                                vfs::evdev::submit(mouse_evdev,ev);
                            }

                            if((packet.buttons & MOUSE_RB) && !(last_pack.buttons & MOUSE_RB)) {
                                ev.code = 273;
                                ev.type = 1;
                                ev.value = 1;
                                vfs::evdev::submit(mouse_evdev,ev);
                            } else if(!(packet.buttons & MOUSE_RB) && (last_pack.buttons & MOUSE_RB)) {
                                ev.code = 273;
                                ev.type = 1;
                                ev.value = 0;
                                vfs::evdev::submit(mouse_evdev,ev);
                            }

                            if((packet.buttons & MOUSE_MB) && !(last_pack.buttons & MOUSE_MB)) {
                                ev.code = 274;
                                ev.type = 1;
                                ev.value = 1;
                                vfs::evdev::submit(mouse_evdev,ev);
                            } else if(!(packet.buttons & MOUSE_MB) && (last_pack.buttons & MOUSE_MB)) {
                                ev.code = 274;
                                ev.type = 1;
                                ev.value = 0;
                                vfs::evdev::submit(mouse_evdev,ev);
                            }

                            asm volatile("sti");

                            last_pack = packet;

                            mouse_seq = 0;

                        }
                        
                    } else {
                        userspace_fd_t fd;
                        memset(&fd,0,sizeof(userspace_fd_t));
                        memcpy(fd.path,"/dev/masterps2keyboard",strlen("/dev/masterps2keyboard"));
                        fd.is_cached_path = 1;

                        std::uint64_t current_nano = drivers::tsc::currentnano();

                        asm volatile("cli");
                        input_event ev;
                        ev.time.tv_sec = current_nano / 1000000000;
                        ev.time.tv_usec = (current_nano & 1000000000) / 1000;
                        ev.type = 1;
                        ev.code = data & ~(1 << 7);
                        ev.value = (data & (1 << 7)) ? 0 : 1;
                        vfs::evdev::submit(kbd_evdev,ev);
                        vfs::vfs::write(&fd,&data,1);
                        asm volatile("sti");
                    }

                    status = io.inb(0x64);
                }
                asm volatile("cli");
                yield();
                asm volatile("sti");
            }
        }
    }
}

void drivers::ps2::init() {
    ps2_flush();
    ps2_writecmd(CMD_READ_CONFIG);
    std::uint8_t ctrl = (std::uint8_t)ps2_read_data();
    if(ps2_is_timeout) {
        Log::Display(LEVEL_MESSAGE_FAIL,"ps2: there's no ps2 controller !\n");
        return;
    }

    ps2_writecmd(0xAD);
    ps2_writecmd(0xA7);

    ps2_writecmd(0x20);
    uint8_t config = ps2_read_data();
    config |= (1 << 0) | (1 << 1) | (1 << 6);
    ps2_writecmd(0x60);
    ps2_writedata(config);

    ps2_enableport(1);
    ps2_enableport(2);

    ps2_writecmd(0xAE);
    ps2_writecmd(0xA8);

    ps2_flush();

    mouse_evdev = vfs::evdev::create("Generic PS/2 Mouse",EVDEV_TYPE_MOUSE);
    kbd_evdev = vfs::evdev::create("Generic PS/2 Keyboard",EVDEV_TYPE_KEYBOARD);

    arch::x86_64::scheduling::create_kernel_thread(ps2_process,0);
    Log::Display(LEVEL_MESSAGE_INFO,"PS/2 Initializied\n");
}