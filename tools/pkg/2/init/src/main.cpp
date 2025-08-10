
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

#include <orange/dev.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <font.hpp>

extern "C" void* __flanterm_malloc(size_t size) {
    return malloc(size);
}

extern "C" void __flanterm_free(void* ptr,size_t size) {
    free(ptr);
}

/* My init will contain fbdev,tty initialization */

uint32_t bg = 0;
uint32_t fg = 0xEEEEEEEE;
uint32_t bright_fg = 0xFFFFFFFF;

class tty {
public:
    static void init() {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        int fb0_dev = open("/dev/fb0",O_RDWR);
        ioctl(fb0_dev,FBIOGET_VSCREENINFO,&vinfo);
        ioctl(fb0_dev,FBIOGET_FSCREENINFO,&finfo);
        void* address = (unsigned char *)mmap(0, vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8,
                                        PROT_READ | PROT_WRITE, MAP_SHARED, fb0_dev, 0);
        memset(address,0,vinfo.xres * finfo.line_length);
        struct flanterm_context *ft_ctx = flanterm_fb_init(
            __flanterm_malloc,
            __flanterm_free,
            (uint32_t*)address, vinfo.xres,vinfo.yres,
            finfo.line_length,
            vinfo.red.length,vinfo.red.offset,
            vinfo.green.length, vinfo.green.offset,
            vinfo.blue.length, vinfo.blue.offset,
            NULL,
            NULL, NULL,
            &bg, &fg,
            &bg, &bright_fg,
            unifont_arr, FONT_WIDTH, FONT_HEIGHT, 0,
            //0, 0, 0, 0,
            1,1,
            0
        );
        
        int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
        grantpt(master_fd);
        unlockpt(master_fd);
        char* name = ptsname(master_fd);
        int slave_fd = open(name,O_RDWR);
        int pid = fork();
        
        if(pid == 0) {
            char buffer[16];
            while(1) {
                memset(buffer,0,16);
                int count = read(master_fd,buffer,16);
                if(count) {
                    flanterm_write(ft_ctx,buffer,count);
                }
            }
        }
        
        dup2(slave_fd,STDOUT_FILENO);
        dup2(slave_fd,STDIN_FILENO);
        dup2(slave_fd,STDERR_FILENO);
    }
};

class fbdev {
public:
    static void init() {
        struct limine_framebuffer fb;
        liborange_access_framebuffer(&fb);
        liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_PIPE_DEV << 32) | 0, "/fb", "/masterfb");
        liborange_setup_mmap("/fb0",fb.address,fb.pitch * fb.width,PTE_WC);
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        liborange_setup_ioctl("/fb0",sizeof(struct fb_var_screeninfo),FBIOGET_VSCREENINFO,0x1);
        liborange_setup_ioctl("/fb0",sizeof(struct fb_fix_screeninfo),FBIOGET_FSCREENINFO,0x2);
        int fb0_dev = open("/dev/fb0",O_RDWR);
        vinfo.red.length = fb.red_mask_size;
        vinfo.red.offset = fb.red_mask_shift;
        vinfo.blue.offset = fb.blue_mask_shift;
        vinfo.blue.length = fb.blue_mask_size;
        vinfo.green.length = fb.green_mask_size;
        vinfo.green.offset = fb.green_mask_shift;
        vinfo.xres = fb.width;
        vinfo.yres = fb.height;
        vinfo.bits_per_pixel = fb.bpp < 5 ? (fb.bpp * 8) : fb.bpp;
        vinfo.xres_virtual = fb.width;
        vinfo.yres_virtual = fb.height;
        finfo.line_length = fb.pitch;
        finfo.smem_len = fb.pitch * fb.height;
        ioctl(fb0_dev,0x1,&vinfo);
        ioctl(fb0_dev,0x2,&finfo);
        close(fb0_dev);
    }
};

int main() {
    fbdev::init();
    tty::init();
    printf("All is working with posix_openpt() !\n");
} 