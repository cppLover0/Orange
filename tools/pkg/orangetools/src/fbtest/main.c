#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <termios.h>
#include <signal.h>


int fb_fd;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
unsigned char *fb_ptr;


void init_fb() {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening framebuffer device");
        exit(1);
    }


    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);


    fb_ptr = (unsigned char *)mmap(0, vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8,
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((uint64_t)fb_ptr == -1) {
        perror("Error mapping framebuffer device to memory");
        exit(3);
    }
}


void draw_pixel(int x, int y, unsigned int color) {
    long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                    (y + vinfo.yoffset) * finfo.line_length;
    *((unsigned int *)(fb_ptr + location)) = color;
}


void cleanup() {
    //munmap(fb_ptr, vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8);
    close(fb_fd);
}


void set_raw_mode() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &newt);
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}


void reset_terminal() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &newt);
    newt.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}


int main() {
    init_fb();
    set_raw_mode();

    for (int y = 0; y < vinfo.yres; y++) {
        for (int x = 0; x < vinfo.xres; x++) {
            draw_pixel(x, y, 0x00FF00); 
        }
    }

    printf("Press any key to exit...\n");
    getchar();

    reset_terminal();
    cleanup();
    return 0;
}