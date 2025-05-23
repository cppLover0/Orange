
#include <stdint.h>
#include <arch/x86_64/interrupts/idt.hpp>

#define STAR_MSR 0xC0000081
#define LSTAR 0xC0000082
#define STAR_MASK 0xC0000084
#define EFER 0xC0000080

typedef struct {
    uint32_t num;
    int (*func)(int_frame_t* frame);
} syscall_t;

#define S_IFMT 0x0F000
#define S_IFBLK 0x06000
#define S_IFCHR 0x02000
#define S_IFIFO 0x01000
#define S_IFREG 0x08000
#define S_IFDIR 0x04000
#define S_IFLNK 0x0A000
#define S_IFSOCK 0x0C000

struct timespec {
	long tv_sec;
	long tv_nsec;
};

typedef unsigned long dev_t;

typedef struct {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    unsigned int st_mode;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned int __pad0;
    unsigned long st_rdev;
    long st_size;
    long st_blksize;
    long st_blocks;

} __attribute__((packed)) stat_t;

class Syscall {
public:
    static void Init();
};