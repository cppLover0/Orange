
// i will not use a per cpu idt, so idt will be for all cpus

#pragma once

#include <stdint.h>

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

typedef struct {
    uint16_t base_low;
    uint16_t cs;
    uint8_t ist;
    uint8_t attr;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint64_t cr3;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vec;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) int_frame_t;
 
class IDT {
public:
    static void Init();
    static idt_entry_t* SetEntry(uint8_t vec,void* base,uint8_t flags);
    static uint8_t AllocEntry();
    static void FreeEntry(uint8_t vec);
    static void Load();
};