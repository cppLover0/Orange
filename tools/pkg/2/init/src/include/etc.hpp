
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

#include <pthread.h>

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

#define DIR_PATH "/etc/drivers/"
#define EXT ".sys"
