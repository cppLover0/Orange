#include <linux/fb.h>
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

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#ifndef FBDEV_ORANGETTY
#define FBDEV_ORANGETTY

class FBDev {
private:
    uint8_t* address;
    int fb_fd;
    struct winsize wininfo;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    struct flanterm_context* ft_ctx;
public:

    uint8_t* get_address() {
        return address;
    } 

    int get_fd() {
        return fb_fd;
    }

    void set_ft_ctx(struct flanterm_context* ft_ctx0) {
        ft_ctx = ft_ctx0;
    }

    struct flanterm_context* get_ft_ctx() {
        return ft_ctx;
    }

    struct fb_var_screeninfo get_vinfo() {
        return vinfo;
    }

    struct fb_fix_screeninfo get_finfo() {
        return finfo;
    }

    void WriteString(const char* str) {
        flanterm_write(ft_ctx,str,strlen(str));
    }

    void WriteChar(char c) {
        flanterm_write(ft_ctx,&c,1);
    }

    void WriteData(char* data,int len) {
        flanterm_write(ft_ctx,data,len);
    }

    void Draw(int x,int y,uint32_t color) {
        long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                    (y + vinfo.yoffset) * finfo.line_length;
        *((unsigned int *)(address+ location)) = color;
    }

    void CreateWinSizeInfo() {
        memset(&wininfo,0,sizeof(struct winsize));
        size_t cols = 0;
        size_t rows = 0;
        flanterm_get_dimensions(ft_ctx,&cols,&rows);
        wininfo.ws_col = cols;
        wininfo.ws_row = rows;
    }

    void SendWinSizeInfo() {
        ioctl(STDIN_FILENO,TIOCSWINSZ,&wininfo);
    }

    FBDev() {
        fb_fd = open("/dev/fb0", O_RDWR);
        if (fb_fd == -1) {
            perror("can't open framebuffer !\n");
            exit(-1);
        }


        ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);


        address = (unsigned char *)mmap(0, vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8,
                                        PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        if((uint64_t)address == -1) {
            perror("can't mmap framebuffer !\n");
            exit(-1);
        }
    }


};

#endif