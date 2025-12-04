
// liborange dev.h 

#include <stdint.h>

#ifndef LIBORANGE_DEV_H
#define LIBORANGE_DEV_H

#define DEVFS_PACKET_CREATE_DEV 1
#define DEVFS_PACKET_READ_READRING 2
#define DEVFS_PACKET_WRITE_READRING 3 
#define DEVFS_PACKET_READ_WRITERING 4 
#define DEVFS_PACKET_WRITE_WRITERING 5
#define DEVFS_PACKET_CREATE_IOCTL 6 
#define DEVFS_PACKET_SIZE_IOCTL 7
#define DEVFS_PACKET_IOCTL 8
#define DEVFS_ENABLE_PIPE 9
#define DEVFS_SETUP_MMAP 10
#define DEVFS_PACKET_CREATE_PIPE_DEV 11
#define DEVFS_PACKET_ISATTY 12

#define DEVFS_GETSLAVE_BY_MASTER 14
#define DEVFS_PACKET_SETUP_RING_SIZE 15

typedef struct {
    union {
        struct {
            uint8_t request;
            uint8_t* cycle;
            uint32_t* queue;
            uint32_t size;
            uint64_t value;
        };
        struct {
            uint8_t request;
            uint64_t ioctlreq;
            uint64_t arg;
        } ioctl;
        struct {
            uint8_t request;
            uint32_t writereg;
            uint32_t readreg;
            uint32_t size;
            uint64_t pointer;
        } create_ioctl;
        struct {
            uint8_t request;
            uint8_t pipe_target;
            uint64_t pipe_pointer;
        } enable_pipe;
        struct {
            uint8_t request;
            uint64_t dma_addr;
            uint64_t size;
        } setup_mmap;
    };
} __attribute__((packed)) devfs_packet_t;

struct limine_video_mode {
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};

struct limine_framebuffer {
    uint64_t address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void* edid;
    uint64_t reserved[4];
};

#define PTE_PRESENT (1 << 0)
#define PTE_RW (1 << 1)
#define PTE_USER (1 << 2)
#define PTE_WC ((1 << 7) | (1 << 3))
#define PTE_MMIO (1ull << 4)

inline void liborange_create_dev(unsigned long long request, char* slave_path, char* master_path) {
    asm volatile("syscall" : : "a"(18), "D"(request), "S"(slave_path), "d"(master_path) : "rcx","r11");
}

inline static void liborange_setup_iopl_3() {
    asm volatile("syscall" : : "a"(19) : "rcx","r11");
}

// char* path, std::uint64_t write_and_read_req, std::uint32_t size
inline void liborange_setup_ioctl(char* path, uint64_t size, int32_t read_req, int32_t write_req) {
    asm volatile("syscall" : : "a"(21), "D"(path), "S"((uint64_t)((uint64_t)write_req << 32) | read_req), "d"(size) : "rcx","r11");
}

inline void liborange_setup_tty(char* path) {
    asm volatile("syscall" : : "a"(22), "D"(path) : "rcx","r11");
}

inline void liborange_setup_mmap(char* path, uint64_t addr, uint64_t size, uint64_t flags) {
    register uint64_t r8 asm("r8") = flags;
    asm volatile("syscall" : : "a"(24), "D"(path), "S"(addr), "d"(size), "r"(flags) : "rcx","r11");
}

inline void liborange_access_framebuffer(struct limine_framebuffer* out) {
    asm volatile("syscall" : : "a"(25), "D"(out) : "rcx", "r11");
}

inline void liborange_setup_ring_bytelen(char* path, int bytelen) {
    asm volatile("syscall" : : "a"(27), "D"(path), "S"(bytelen) : "rcx","r11");
}

inline uint64_t liborange_alloc_dma(uint64_t dma_size) {
    uint64_t addr = 0;
    asm volatile("syscall" : "=d"(addr) : "a"(38), "D"(dma_size) : "rcx","r11");
    return addr;
}

void liborange_free_dma(uint64_t dma_addr) {
    asm volatile("syscall" : : "a"(40), "D"(dma_addr) : "rcx","r11");
}

inline void* liborange_map_phys(uint64_t phys_addr,uint64_t flags,uint64_t size) {
    uint64_t addr = 0;
    asm volatile("syscall" : "=d"(addr) : "a"(39), "D"(phys_addr), "S"(flags), "d"(size) : "rcx","r11");
    return (void*)addr;
}

#define PAGE_SIZE 4096

#define ROUNDUP(VALUE,ROUND) ((VALUE + (ROUND - 1)) / ROUND)
#define ALIGNPAGEUP(VALUE) (ROUNDUP(VALUE,PAGE_SIZE) * PAGE_SIZE)
#define ALIGNPAGEDOWN(VALUE) ((VALUE / PAGE_SIZE) * PAGE_SIZE)

#define CROUNDUP(VALUE,ROUND) ((VALUE + (ROUND - 1)) / ROUND)
#define CALIGNPAGEUP(VALUE,c) ((VALUE + c - 1) & ~(c - 1))
#define CALIGNPAGEDOWN(VALUE,c) ((VALUE / c) * c)

typedef struct {
    uint16_t irq;
    uint64_t flags;
} __attribute__((packed)) liborange_pic_create_t;

#endif