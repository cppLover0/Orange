#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main() {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd == -1) return 1;

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        close(fd);perror("mezow");
        return 1;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        close(fd);perror("mxeow");
        return 1;
    }

    size_t size = finfo.smem_len;
    char *fbp = (char *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (fbp == MAP_FAILED) {
        close(fd);
        perror("meow");
        return 1;
    }

    memset(fbp, 0, size);
    printf("wasss\n");
    munmap(fbp, size);
    close(fd);
    return 0;
}
