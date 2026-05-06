#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define BITS_PER_LONG (8 * sizeof(long))
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define TEST_BIT(bit, array) ((array[(bit)/BITS_PER_LONG] >> ((bit)%BITS_PER_LONG)) & 1)

typedef struct {
    int fd;
    uint32_t *ptr;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} fb_t;

int is_mouse(int fd) {
    unsigned long evbit[NBITS(EV_MAX)] = {0};
    unsigned long keybit[NBITS(KEY_MAX)] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) return 0;
    if (TEST_BIT(EV_REL, evbit) && TEST_BIT(EV_KEY, evbit)) {
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) return 0;
        if (TEST_BIT(BTN_LEFT, keybit)) return 1;
    }
    return 0;
}

int find_mouse() {
    DIR *dir = opendir("/dev/input");
    struct dirent *ent;
    char path[256];
    printf("brekpoinvt\n");
    if (!dir) return -1;
    printf("brekpo1int\n");
    while ((ent = readdir(dir))) {
        printf("brekpoint %s %s \n", ent->d_name, "event");
        if (strncmp(ent->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            printf("brekpo1int !!!!!\n");
            if (fd >= 0) {
                printf("brekpo1int,\n");
                if (is_mouse(fd)) {
                    printf("brekpo1int111231\n");
                    closedir(dir);
                    return fd;
                }
                printf("brekpo1infffffffffft\n");
                close(fd);
            }
        }
    }
    closedir(dir);
    return -1;
}

void draw_pixel(fb_t *fb, int x, int y, uint32_t color) {
    if (x >= 0 && x < fb->width && y >= 0 && y < fb->height)
        fb->ptr[y * fb->stride + x] = color;
}

int main() {
    int ev_fd = find_mouse();

    printf("brekpoint\n");
    if (ev_fd < 0) return 1;

    printf("brekpoint\n");
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) return 1;
printf("brekpoinxt\n");
    struct fb_var_screeninfo vinfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

    fb_t fb = {
        .fd = fb_fd,
        .width = vinfo.xres,
        .height = vinfo.yres,
        .stride = vinfo.xres,
        .ptr = mmap(0, vinfo.xres * vinfo.yres * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0)
    };

    memset(fb.ptr, 0, fb.width * fb.height * 4);

    int cur_x = fb.width / 2, cur_y = fb.height / 2;
    struct input_event ev;

    while (1) {
        while (read(ev_fd, &ev, sizeof(ev)) > 0) {
            if (ev.type == EV_REL) {
                if (ev.code == REL_X) cur_x += ev.value;
                if (ev.code == REL_Y) cur_y += ev.value;
            } else if (ev.type == EV_KEY && ev.code == BTN_LEFT && ev.value == 1) {
                memset(fb.ptr, 0, fb.width * fb.height * 4);
            }
            if (cur_x < 0) cur_x = 0; if (cur_x >= fb.width) cur_x = fb.width - 1;
            if (cur_y < 0) cur_y = 0; if (cur_y >= fb.height) cur_y = fb.height - 1;
            draw_pixel(&fb, cur_x, cur_y, 0xFFFFFFFF);
        }
        usleep(1000);
    }

    return 0;
}
