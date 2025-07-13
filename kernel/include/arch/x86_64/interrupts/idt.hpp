
#include <cstdint>

#pragma once

typedef struct {
    std::uint16_t low;
    std::uint16_t cs;
    std::uint8_t ist;
    std::uint8_t attr;
    std::uint16_t mid;
    std::uint32_t high;
    std::uint32_t reserved0;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    std::uint16_t limit;
    std::uint64_t base;
} __attribute__((packed)) idtr_t;

typedef struct {
    std::uint64_t cr3;
    std::uint64_t rax;
    std::uint64_t rbx;
    std::uint64_t rcx;
    std::uint64_t rdx;
    std::uint64_t rsi;
    std::uint64_t rdi;
    std::uint64_t rbp;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t r10;
    std::uint64_t r11;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint64_t vec;
    std::uint64_t err_code;
    std::uint64_t rip;
    std::uint64_t cs;
    std::uint64_t rflags;
    std::uint64_t rsp;
    std::uint64_t ss;
} __attribute__((packed)) int_frame_t;

namespace arch {
    namespace x86_64 {
        namespace interrupts {
            class idt {
            public:
                static void init();
                static void set_entry(std::uint64_t base,std::uint8_t vec,std::uint8_t flags,std::uint8_t ist);
                static std::uint8_t alloc();

                static void load();
            };
        };
    };
};

