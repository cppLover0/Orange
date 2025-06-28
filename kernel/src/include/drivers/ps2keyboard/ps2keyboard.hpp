
#include <stdint.h>
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>

#pragma once

#define PS2K_PNP_ID "PNP0303"
#define SHIFT_PRESSED 0x2A
#define SHIFT_RELEASED 0xAA

typedef struct ps2keyboard_pipe_struct {
    _Atomic char is_used_anymore;
    pipe_t* pipe;
    struct ps2keyboard_pipe_struct* next;
} __attribute__((packed)) ps2keyboard_pipe_struct_t;

class PS2Keyboard {
public:
    static void Init();
    static void EOI();
    static short Get();
};