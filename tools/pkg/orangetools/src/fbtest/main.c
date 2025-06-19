#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>


int main() {
    int fb_fd = open("/dev/fb0", O_RDWR);
    struct fb_var_screeninfo vinfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    long screensize = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
    char *fb_ptr = malloc(screensize);
    memset(fb_ptr, 0xFF, screensize);
    write(fb_fd, fb_ptr, screensize);
    free(fb_ptr);
    close(fb_fd);
    struct fb_fix_screeninfo i;
    FBIOGET_FSCREENINFO;
    return 0;
}