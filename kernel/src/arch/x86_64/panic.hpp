#pragma once
#include <cstdint>
#include <arch/x86_64/cpu/idt.hpp>

typedef struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} __attribute__((packed)) stackframe_t;

namespace x86_64 {
    namespace panic {
        void print_ascii_art();
        void print_regs(x86_64::idt::int_frame_t* ctx);
        extern "C" void CPUKernelPanic(x86_64::idt::int_frame_t* frame);
    };
};