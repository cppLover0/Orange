
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

class Syscall {
public:
    static void Init();
};