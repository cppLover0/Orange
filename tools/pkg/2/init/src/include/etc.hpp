
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

#include <poll.h>
#include <fcntl.h>

#include <dirent.h>
#include <string.h>
#include <sys/types.h>

#include <orange/dev.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <font.hpp>

#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void scale_image_nn(unsigned char* src, int src_w, int src_h,
                    unsigned char* dst, int dst_w, int dst_h, int channels) {
    for (int y = 0; y < dst_h; y++) {
        int src_y = y * src_h / dst_h;
        for (int x = 0; x < dst_w; x++) {
            int src_x = x * src_w / dst_w;
            for (int c = 0; c < channels; c++) {
                dst[(y*dst_w + x)*channels + c] = src[(src_y*src_w + src_x)*channels + c];
            }
        }
    }
}

uint32_t rgb_to_pixel(uint8_t r, uint8_t g, uint8_t b,
                      int r_len, int r_off,
                      int g_len, int g_off,
                      int b_len, int b_off) {
    uint32_t pixel = 0;
    uint32_t R = (r >> (8 - r_len)) << r_off;
    uint32_t G = (g >> (8 - g_len)) << g_off;
    uint32_t B = (b >> (8 - b_len)) << b_off;
    pixel = R | G | B;
    return pixel;
}

static uint32_t blend_pixel(uint32_t dst, uint8_t r_src, uint8_t g_src, uint8_t b_src, float alpha,
                            int r_len, int r_off, int g_len, int g_off, int b_len, int b_off) {
    uint8_t r_dst = (dst >> r_off) & ((1 << r_len) - 1);
    uint8_t g_dst = (dst >> g_off) & ((1 << g_len) - 1);
    uint8_t b_dst = (dst >> b_off) & ((1 << b_len) - 1);

    r_dst = (r_dst << (8 - r_len)) | (r_dst >> (2 * r_len - 8));
    g_dst = (g_dst << (8 - g_len)) | (g_dst >> (2 * g_len - 8));
    b_dst = (b_dst << (8 - b_len)) | (b_dst >> (2 * b_len - 8));

    uint8_t r = (uint8_t)(r_src * alpha + r_dst * (1.0f - alpha));
    uint8_t g = (uint8_t)(g_src * alpha + g_dst * (1.0f - alpha));
    uint8_t b = (uint8_t)(b_src * alpha + b_dst * (1.0f - alpha));

    uint32_t pixel = 0;
    pixel |= ((r >> (8 - r_len)) << r_off);
    pixel |= ((g >> (8 - g_len)) << g_off);
    pixel |= ((b >> (8 - b_len)) << b_off);

    return pixel;
}

static void draw_transparent_black_square(uint32_t* canvas, int width, int height,
                                          int r_len, int r_off,
                                          int g_len, int g_off,
                                          int b_len, int b_off,
                                          int margin) {
    int square_w = width - 2 * margin;
    int square_h = height - 2 * margin;
    int start_x = margin;
    int start_y = margin;
    float alpha = 0.5f; 

    for (int y = start_y; y < start_y + square_h; y++) {
        for (int x = start_x; x < start_x + square_w; x++) {
            int idx = y * width + x;

            canvas[idx] = blend_pixel(canvas[idx], 0, 0, 0, alpha,
                                     r_len, r_off, g_len, g_off, b_len, b_off);
        }
    }
}

void debug_log(const char* msg) {
    asm volatile("syscall" : : "a"(9), "D"(msg) : "rcx","r11");
}

extern "C" void* __flanterm_malloc(size_t size) {
    return malloc(size);
}

extern "C" void __flanterm_free(void* ptr,size_t size) {
    free(ptr);
}

/* My init will contain fbdev,tty,input0 initialization */

uint32_t default_fg = 0xEEEEEEEE;
uint32_t default_fg_bright = 0xFFFFFFFF;

char is_shift_pressed = 0;
char is_ctrl_pressed = 0;

const char en_layout_translation[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

const char en_layout_translation_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};
\
#define ASCII_CTRL_OFFSET 96

#define SHIFT_PRESSED 0x2A
#define SHIFT_RELEASED 0xAA
#define CTRL_PRESSED 29
#define CTRL_RELEASED 157

char* keybuffer;
int key_ptr = 0;

int is_printable(char c) {
    return (c >= 32 && c <= 126) || c == 10; 
}

class tty {
public:

    static void doKeyWork(uint8_t key,int master_fd) {
            struct termios tty_termios;
            tcgetattr(0,&tty_termios);

            if(key == SHIFT_PRESSED) {
                is_shift_pressed = 1;
                return;
            } else if(key == SHIFT_RELEASED) {
                is_shift_pressed = 0;
                return;
            }

            if(key == CTRL_PRESSED) {
                is_ctrl_pressed = 1;
                return;
            } else if(key == CTRL_RELEASED) {
                is_ctrl_pressed = 0;
                return;
            }

            if(!(key & (1 << 7))) {
                char layout_key;

                if(!is_shift_pressed)
                    layout_key = en_layout_translation[key];
                else
                    layout_key = en_layout_translation_shift[key];

                if(is_ctrl_pressed)
                    layout_key = en_layout_translation[key] - ASCII_CTRL_OFFSET;

                if((tty_termios.c_lflag & ECHO) && is_printable(layout_key))
                    write(STDOUT_FILENO,&layout_key,1);

                if((tty_termios.c_lflag & ICANON) && (is_printable(layout_key) || layout_key == '\b') && layout_key != 0) {
                                
                    if(layout_key != '\b') {
                        keybuffer[key_ptr++] = layout_key;
                        if(layout_key == '\n') {
                            write(master_fd,keybuffer,key_ptr);
                            key_ptr = 0;
                        }
                    } else {
                        if(key_ptr) {
                            write(STDOUT_FILENO,"\b \b",sizeof("\b \b"));
                            keybuffer[key_ptr--] = '\0';
                        }
                    }

                } else {
                                
                    if(layout_key == '\n')
                        layout_key = 13;

                    keybuffer[key_ptr++] = layout_key;

                    if(key_ptr >= tty_termios.c_cc[VMIN]) {
                        write(master_fd,keybuffer,key_ptr);
                        key_ptr = 0;
                    }

            }
         }
    }

    static void init() {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        int fb0_dev = open("/dev/fb0", O_RDWR);
        ioctl(fb0_dev, FBIOGET_VSCREENINFO, &vinfo);
        ioctl(fb0_dev, FBIOGET_FSCREENINFO, &finfo);

        void* address = mmap(0, vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8,
                            PROT_READ | PROT_WRITE, MAP_SHARED, fb0_dev, 0);

        uint32_t* test_canvas = (uint32_t*)malloc(vinfo.yres_virtual * vinfo.xres_virtual * sizeof(uint32_t));
        if (!test_canvas) {
            exit(1);
        }

        int img_w, img_h, img_channels;
        unsigned char* img_data = stbi_load("/etc/bg.jpg", &img_w, &img_h, &img_channels, 3);
        if (!img_data) {
            exit(2);
        }

        unsigned char* scaled_rgb = (unsigned char*)malloc(vinfo.xres * vinfo.yres * 3);
        if (!scaled_rgb) {
            exit(3);
        }

        scale_image_nn(img_data, img_w, img_h, scaled_rgb, vinfo.xres, vinfo.yres, 3);

        for (int y = 0; y < vinfo.yres; y++) {
            for (int x = 0; x < vinfo.xres; x++) {
                int idx = (y * vinfo.xres + x) * 3;
                uint8_t r = scaled_rgb[idx];
                uint8_t g = scaled_rgb[idx + 1];
                uint8_t b = scaled_rgb[idx + 2];
                test_canvas[y * vinfo.xres + x] = rgb_to_pixel(r, g, b,
                                                            vinfo.red.length, vinfo.red.offset,
                                                            vinfo.green.length, vinfo.green.offset,
                                                            vinfo.blue.length, vinfo.blue.offset);
            }
        }

        stbi_image_free(img_data);
        free(scaled_rgb);

        int margin = 64; 

        draw_transparent_black_square(test_canvas, vinfo.xres, vinfo.yres,
                                    vinfo.red.length, vinfo.red.offset,
                                    vinfo.green.length, vinfo.green.offset,
                                    vinfo.blue.length, vinfo.blue.offset,
                                    margin - 1);

        struct flanterm_context *ft_ctx = flanterm_fb_init(
            __flanterm_malloc, __flanterm_free,
            (uint32_t*)address, vinfo.xres, vinfo.yres,
            finfo.line_length,
            vinfo.red.length, vinfo.red.offset,
            vinfo.green.length, vinfo.green.offset,
            vinfo.blue.length, vinfo.blue.offset,
            test_canvas,
            NULL, NULL,
            NULL, &default_fg,
            NULL, &default_fg_bright,
            (void*)unifont_arr, FONT_WIDTH, FONT_HEIGHT, 0,
            1, 1, margin
        );
        int master_fd = posix_openpt(O_RDWR | O_NOCTTY);
        grantpt(master_fd);
        unlockpt(master_fd);
        char* name = ptsname(master_fd);
        int slave_fd = open(name,O_RDWR);
        int pid = fork();
        
        if(pid == 0) {
            char buffer[256];
            while(1) {
                memset(buffer,0,256);
                int count = read(master_fd,buffer,256);
                if(count) {
                    flanterm_write(ft_ctx,buffer,count);
                }
            }
        }
        
        dup2(slave_fd,STDOUT_FILENO);
        dup2(slave_fd,STDIN_FILENO);
        dup2(slave_fd,STDERR_FILENO);

        struct winsize buf;

        size_t cols = 0;
        size_t rows = 0;
        flanterm_get_dimensions(ft_ctx,&cols,&rows);
        buf.ws_col = cols;
        buf.ws_row = rows;

        ioctl(STDIN_FILENO,TIOCSWINSZ,&buf);
        struct termios t;

        t.c_lflag |= (ICANON | ECHO);

        t.c_iflag = IGNPAR | ICRNL;
        t.c_oflag = OPOST;         
        t.c_cflag |= (CS8);   

        ioctl(STDIN_FILENO, TCSETS, &t);

        liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_DEV << 32) | 0, "/ps2keyboard", "/masterps2keyboard");
        liborange_setup_ring_bytelen("/ps2keyboard0",1);
        int master_input = open("/dev/masterps2keyboard0", O_RDWR);
        int slave_input = open("/dev/ps2keyboard0", O_RDWR);

        keybuffer = (char*)malloc(1024);

        pid = fork();

        if(pid == 0) {
            struct pollfd pfd;
            pfd.fd = slave_input;
            pfd.events = POLLIN;

            while(1) {
                int ret = poll(&pfd, 1, -1); 
                if(ret > 0) {
                    if(pfd.revents & POLLIN) {
                        char buffer[32];
                        memset(buffer, 0, 32);
                        int count = read(slave_input, buffer, 32);
                        if(count > 0) {
                            for(int i = 0; i < count; i++) {
                                doKeyWork(buffer[i], master_fd);
                            }
                        }
                    }
                }
                
            }
        }
    }
};

#include <emmintrin.h> 

#define LEVEL_MESSAGE_OK 0
#define LEVEL_MESSAGE_FAIL 1
#define LEVEL_MESSAGE_WARN 2
#define LEVEL_MESSAGE_INFO 3

const char* level_messages[] = {
    [LEVEL_MESSAGE_OK] = "[   \x1b[38;2;0;255;0mOK\033[0m   ] ",
    [LEVEL_MESSAGE_FAIL] = "[ \x1b[38;2;255;0;0mFAILED\033[0m ] ",
    [LEVEL_MESSAGE_WARN] = "[  \x1b[38;2;255;165;0mWARN\033[0m  ] ",
    [LEVEL_MESSAGE_INFO] = "[  \x1b[38;2;0;191;255mINFO\033[0m  ] "
};

#define log(level, format,...) printf("%s" format "\n", level_messages[level], ##__VA_ARGS__)

class fbdev {
public:

    static void refresh(void* ptr, void* a) {
        struct limine_framebuffer fb;
        liborange_access_framebuffer(&fb);
        int buffer_in_use = 0;
        void* real_fb = liborange_map_phys(fb.address, PTE_WC, fb.pitch * fb.height);

        while(1) {
            memcpy(real_fb,ptr,fb.height * fb.pitch);
        }
    }

    static void setup_ioctl(int fb0_dev ,struct limine_framebuffer fb) {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
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
        vinfo.red.msb_right = 1;
        vinfo.green.msb_right = 1;
        vinfo.blue.msb_right = 1;
        vinfo.transp.msb_right = 1;
        vinfo.height = -1;
        vinfo.width = -1;
        finfo.line_length = fb.pitch;
        finfo.smem_len = fb.pitch * fb.height;
        finfo.visual = FB_VISUAL_TRUECOLOR;
        finfo.type = FB_TYPE_PACKED_PIXELS;
        finfo.mmio_len = fb.pitch * fb.height;
        ioctl(fb0_dev,0x1,&vinfo);
        ioctl(fb0_dev,0x2,&finfo);
    }

    static void init() {
        struct limine_framebuffer fb;
        liborange_access_framebuffer(&fb);
        liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_PIPE_DEV << 32) | 0, "/fb", "/masterfb");
        liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_PIPE_DEV << 32) | 0, "/vbe", "/mastervbe");

        uint64_t doublebuffer_dma = liborange_alloc_dma(fb.pitch * fb.height);
        void* mapped_dma = liborange_map_phys(doublebuffer_dma,0,fb.pitch * fb.height);

        void* a = aligned_alloc(4096,fb.pitch * fb.height);
        void* b = aligned_alloc(4096,fb.pitch * fb.height);

        int pid = fork();
        if(pid == 0) {
            refresh(mapped_dma,a);
        }

        liborange_setup_mmap("/fb0",doublebuffer_dma,fb.pitch * fb.height,0);
        
        liborange_setup_ioctl("/fb0",sizeof(struct fb_var_screeninfo),FBIOGET_VSCREENINFO,0x1);
        liborange_setup_ioctl("/fb0",sizeof(struct fb_fix_screeninfo),FBIOGET_FSCREENINFO,0x2);
        int fb0_dev = open("/dev/fb0",O_RDWR);
        
        setup_ioctl(fb0_dev,fb);
        
        close(fb0_dev);
    }

};

#define DIR_PATH "/etc/drivers/"
#define EXT ".sys"
