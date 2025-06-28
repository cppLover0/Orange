
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <pty.h>
#include <iostream>

#ifndef ORANGETTY_TTY
#define ORANGETTY_TTY

#include <orangettyconfig.hpp>

class TTY {
private:
    struct termios tty_termios;
    uint32_t tty_num;
    char* tty;
public:

    TTY() {
        tty_num = 0;
    }

    uint32_t get_tty() {
        return tty_num;
    }

    char* get_tty_name() {
        return tty;
    }

    void Fetch() {
        int fd = open("/dev/ttx",O_RDWR);
        ioctl(fd,TTX_CREATE,&tty_num);
        close(fd);
        tty = (char*)malloc(256);
    }

    void SetTTY(int fd) {
        memset(tty,0,256);
        sprintf(tty, "/dev/tty%d", tty_num);

        int ret;
        asm volatile("syscall" : "=a"(ret) : "a"(44), "D"(fd), "S"(tty), "d"(strlen(tty)) : "rcx","r11");
    }

    void SetupTermios() {
        memset(&tty_termios,0,sizeof(struct termios));
        tty_termios.c_cc[VMIN] = 1;
        tty_termios.c_lflag = ECHO | ICANON;
        tty_termios.c_cflag = CS8 | CREAD | HUPCL;
        tty_termios.c_oflag = OPOST | ONLCR;
        tty_termios.c_iflag = ICRNL | IXON;
        tcsetattr(STDIN_FILENO,TCSANOW,&tty_termios);
    }

    static void DisableOSHelp() {
        asm volatile("syscall" : : "a"(45) : "rcx","r11");
    }

};

#endif