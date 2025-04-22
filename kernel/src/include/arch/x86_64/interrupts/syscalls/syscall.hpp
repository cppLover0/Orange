
#include <stdint.h>
#include <arch/x86_64/interrupts/idt.hpp>

#define STAR_MSR 0xC0000081
#define LSTAR 0xC0000082
#define STAR_MASK 0xC0000084
#define EFER 0xC0000080

typedef struct fd_struct {
    int index;
    char is_in_use;
    long seek_offset;
    struct fd_struct* next;
    char path[4075]; // align 4k
} __attribute__((packed)) fd_t;

typedef struct {
    uint32_t num;
    int (*func)(int_frame_t* frame);
} syscall_t;

class Syscall {
public:
    static void Init();
};