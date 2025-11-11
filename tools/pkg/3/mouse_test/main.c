#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
    int fd;
    char *fbp;
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;
    long int screensize;
    int cursor_x, cursor_y;
    int cursor_visible;
} fb_info_t;

int init_fb(fb_info_t *fb) {
    fb->fd = open("/dev/fb0", O_RDWR);
    if (fb->fd == -1) {
        perror("Error: cannot open framebuffer device");
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->finfo)) {
        perror("Error reading fixed information");
        close(fb->fd);
        return -1;
    }

    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vinfo)) {
        perror("Error reading variable information");
        close(fb->fd);
        return -1;
    }

    fb->screensize = fb->vinfo.yres_virtual * fb->finfo.line_length;

    fb->fbp = (char *)mmap(0, fb->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if ((long)fb->fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        close(fb->fd);
        return -1;
    }

    fb->cursor_x = fb->vinfo.xres / 2;
    fb->cursor_y = fb->vinfo.yres / 2;
    fb->cursor_visible = 1;

    printf("Framebuffer initialized: %dx%d, %dbpp\n", 
           fb->vinfo.xres, fb->vinfo.yres, fb->vinfo.bits_per_pixel);

    return 0;
}

void put_pixel(fb_info_t *fb, int x, int y, unsigned int color) {
    if (x < 0 || x >= fb->vinfo.xres || y < 0 || y >= fb->vinfo.yres)
        return;

    long location = (x + fb->vinfo.xoffset) * (fb->vinfo.bits_per_pixel / 8) +
                    (y + fb->vinfo.yoffset) * fb->finfo.line_length;

    if (fb->vinfo.bits_per_pixel == 32) {
        *((unsigned int *)(fb->fbp + location)) = color;
    } else if (fb->vinfo.bits_per_pixel == 16) {
        *((unsigned short *)(fb->fbp + location)) = color & 0xFFFF;
    } else {
        *((unsigned char *)(fb->fbp + location)) = color & 0xFF;
    }
}

void draw_cursor(fb_info_t *fb) {
    int x = fb->cursor_x;
    int y = fb->cursor_y;
    unsigned int color = 0xFFFFFF; 

    for (int i = -5; i <= 5; i++) {
        if (i != 0) { 
            put_pixel(fb, x, y + i, color);
        }
    }

    for (int i = -5; i <= 5; i++) {
        if (i != 0) { 
            put_pixel(fb, x + i, y, color);
        }
    }

    put_pixel(fb, x, y, 0xFF0000);
}

void clear_cursor_area(fb_info_t *fb) {
    int size = 10;
    for (int i = -size; i <= size; i++) {
        for (int j = -size; j <= size; j++) {
            put_pixel(fb, fb->cursor_x + i, fb->cursor_y + j, 0x000000);
        }
    }
}

void update_cursor(fb_info_t *fb, int dx, int dy) {

    if (fb->cursor_visible) {
        clear_cursor_area(fb);
    }

    fb->cursor_x += dx;
    fb->cursor_y += dy;

    if (fb->cursor_x < 0) fb->cursor_x = 0;
    if (fb->cursor_x >= fb->vinfo.xres) fb->cursor_x = fb->vinfo.xres - 1;
    if (fb->cursor_y < 0) fb->cursor_y = 0;
    if (fb->cursor_y >= fb->vinfo.yres) fb->cursor_y = fb->vinfo.yres - 1;

    draw_cursor(fb);
}

void clear_screen(fb_info_t *fb) {
    memset(fb->fbp, 0, fb->screensize);
}

int main() {
    int mouse_fd = open("/dev/mouse", O_RDWR);
    if (mouse_fd == -1) {
        perror("Error: cannot open mouse device");
        return 1;
    }

    fb_info_t fb;
    if (init_fb(&fb) == -1) {
        close(mouse_fd);
        return 1;
    }

    clear_screen(&fb);
    
    draw_cursor(&fb);

    struct pollfd fd;
    fd.events = POLLIN;
    fd.fd = mouse_fd;

    while(1) {
        int num = poll(&fd, 1, -1);
        if (num > 0 && (fd.revents & POLLIN)) {
            mouse_packet_t packets[256];
            memset(packets,0,sizeof(mouse_packet_t) * 256);
            int bytes_read = read(mouse_fd, &packets, sizeof(mouse_packet_t) * 256);
            
            for(int i = 0; i < bytes_read / (sizeof(mouse_packet_t));i++) {
                mouse_packet_t packet = packets[i];
                if (1) {
                
                    int dx = (signed char)packet.x;
                    int dy = (signed char)packet.y;
                    
                    
                    update_cursor(&fb, dx, -dy); 
                    if(packet.buttons & MOUSE_LB) {
                        printf("pressed left button\n");
                    }

                    if(packet.buttons & MOUSE_RB) {
                        printf("pressed right button\n");
                    }

                    if(packet.buttons & MOUSE_MB) {
                        printf("pressed middle button\n");
                    }
                    
                }
            }

        }
    }

    return 0;
}