
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
#include <orange/io.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <font.hpp>

#include <etc.hpp>

#include <sys/wait.h>

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void start_all_drivers() {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(DIR_PATH);
    if (!dir) {
        exit(0);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (ends_with(entry->d_name, ".wsys")) {
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s%s", DIR_PATH, entry->d_name);

            log(LEVEL_MESSAGE_OK,"Starting %s",entry->d_name);

            pid_t pid = fork();
            if (pid < 0) {
                continue;
            } else if (pid == 0) {
                execl(filepath, filepath, (char *)NULL);
            } 
        } else if(ends_with(entry->d_name,".sys")) {
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s%s", DIR_PATH, entry->d_name);

            log(LEVEL_MESSAGE_OK,"Starting %s",entry->d_name);

            pid_t pid = fork();
            if (pid != 0) {
                char success = 0;
                while(!success) {
                    int r_pid = wait(NULL);
                    if(pid == r_pid)
                        success = 1;
                }
            } else if (pid == 0) {
                execl(filepath, filepath, (char *)NULL);
            } 
        }
    }

    closedir(dir);
}

char* keybuffer;
int key_ptr = 0;

int is_printable(char c) {
    return (c >= 32 && c <= 126) || c == 10; 
}

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

            write(master_fd,&layout_key,1);

        //     if((tty_termios.c_lflag & ECHO) && is_printable(layout_key))
        //         write(STDOUT_FILENO,&layout_key,1);

        //     if((tty_termios.c_lflag & ICANON) && (is_printable(layout_key) || layout_key == '\b') && layout_key != 0) {
                                
        //         if(layout_key != '\b') {
        //             keybuffer[key_ptr++] = layout_key;
        //             if(layout_key == '\n') {
        //                 write(master_fd,keybuffer,key_ptr);
        //                 key_ptr = 0;
        //             }
        //         } else {
        //             if(key_ptr) {
        //                 write(STDOUT_FILENO,"\b \b",sizeof("\b \b"));
                            
        //                 keybuffer[key_ptr--] = '\0';
        //             }
        //         }
        //     } else {
                                
        //         if(layout_key == '\n')
        //             layout_key = 13;

        //         keybuffer[key_ptr++] = layout_key;

        //         if(key_ptr >= tty_termios.c_cc[VMIN]) {
        //             write(master_fd,keybuffer,key_ptr);
        //             key_ptr = 0;
        //         }
        // }
     }
}

int master_fd = 0;
int slave_fd = 0;
struct flanterm_context* ft_ctx;

void* ptr; // fbdev target 

void* tty_work(void* arg) {

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

    struct pollfd pfd[2];
    pfd[0].fd = slave_input;
    pfd[0].events = POLLIN;
    pfd[1].fd = master_fd;
    pfd[1].events = POLLIN;

    while(1) {
        int ret = poll(pfd, 2, -1); 
        if(ret > 0) {
            if(pfd[0].revents & POLLIN) {
                char buffer[32];
                memset(buffer, 0, 32);
                int count = read(slave_input, buffer, 32);
                if(count > 0) {
                    for(int i = 0; i < count; i++) {
                        doKeyWork(buffer[i], master_fd);
                    }
                }
            }

            if(pfd[1].revents & POLLIN) {
                char buffer[256];
                memset(buffer,0,256);
                int count = read(master_fd,buffer,256);
                if(count) flanterm_write(ft_ctx,buffer,count);
            }
        }
        
    }

}


static void tty_init() {
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

    ft_ctx = flanterm_fb_init(
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
    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    grantpt(master_fd);
    unlockpt(master_fd);
    char* name = ptsname(master_fd);
    slave_fd = open(name,O_RDWR);

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

    liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_DEV << 32) | 0, "/mouse", "/mastermouse");
    liborange_setup_ring_bytelen("/mouse0",4);

    int master_input = open("/dev/masterps2keyboard0", O_RDWR);
    int slave_input = open("/dev/ps2keyboard0", O_RDWR);
    keybuffer = (char*)malloc(1024);
    pthread_t pth;
    pthread_create(&pth,NULL,tty_work,0);

}

static inline void rep_movsb(void* dest, const void* src, size_t count) {
        asm volatile(
            "mov %0, %%rdi\n\t"
            "mov %1, %%rsi\n\t"
            "mov %2, %%rcx\n\t"
            "rep movsb\n\t"
            :
            : "r"(dest), "r"(src), "r"(count)
            : "%rdi", "%rsi", "%rcx", "memory");
}

inline void __cpuid(int code, int code2, uint32_t *a, uint32_t *b, uint32_t *c , uint32_t *d) {
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(code),"c"(code2));
}

int is_running_on_real_hw() {
    int regs[4];

    __asm__ volatile (
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(0)
    );

    char vendor_id[13];
    *(uint32_t*)(vendor_id + 0) = regs[1];
    *(uint32_t*)(vendor_id + 4) = regs[3];
    *(uint32_t*)(vendor_id + 8) = regs[2];
    vendor_id[12] = '\0';

    int is_vendor_virt = strncmp(vendor_id, "AuthenticAMD", 12) == 0 ||
           strncmp(vendor_id, "GenuineIntel", 12) == 0;

    if(is_vendor_virt) {
        uint32_t a,b,c,d = 0;
        __cpuid(0x40000000,0,&a,&b,&c,&d);
        if(b != 0x4b4d564b || d != 0x4d || c != 0x564b4d56)
            return 1;

        __cpuid(0x40000001,0,&a,&b,&c,&d);
        if(!(a & (1 << 3)))
            return 1;
        is_vendor_virt = 0;
    }
    return is_vendor_virt;

}

void get_cpu(char* out) {
    int regs[4];

    __asm__ volatile (
        "cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(0)
    );

    char vendor_id[13];
    *(uint32_t*)(vendor_id + 0) = regs[1];
    *(uint32_t*)(vendor_id + 4) = regs[3];
    *(uint32_t*)(vendor_id + 8) = regs[2];
    vendor_id[12] = '\0';
    memcpy(out,vendor_id,13);
}

void* refresh(void * arg) {
    exit(0);
    struct limine_framebuffer fb;
    liborange_access_framebuffer(&fb);
    void* real_fb = liborange_map_phys(fb.address, PTE_WC, fb.pitch * fb.height);

    while(1) {
        memcpy(real_fb,ptr,fb.pitch * fb.height);
    }
}

void init_fbdev() {
    struct limine_framebuffer fb;
    liborange_access_framebuffer(&fb);
    liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_PIPE_DEV << 32) | 0, "/fb", "/masterfb");

    uint64_t doublebuffer_dma = liborange_alloc_dma(fb.pitch * fb.height);
    void* mapped_dma = liborange_map_phys(doublebuffer_dma,0,fb.pitch * fb.height);

    ptr = mapped_dma;

    liborange_setup_mmap("/fb0",fb.address,fb.pitch * fb.height,PTE_WC);
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
    close(fb0_dev);
}

int main() {
    init_fbdev();
    tty_init();

    char cpu[13];
    get_cpu(cpu);

    log(LEVEL_MESSAGE_INFO,"Current vendor: %s",cpu);
    log(LEVEL_MESSAGE_OK,"Trying to start drivers");
    start_all_drivers();
    log(LEVEL_MESSAGE_OK,"Initialization done");
    printf("\n");
    putenv("TERM=linux");
    putenv("SHELL=/bin/bash");
    putenv("PATH=/usr/bin");
    int pid = fork();
    if(pid == 0)
        system("/bin/bash /etc/init.sh");
    else {
        while(1) {
            asm volatile("nop"); // nothing
        }
    }
} 