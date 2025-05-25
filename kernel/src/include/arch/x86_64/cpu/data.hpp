
#pragma once

#include <lib/limineA/limine.h>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <stdint.h>

typedef struct {
    uint64_t user_stack;
    uint64_t kernel_stack;
    GDT gdt;
    struct LIMINE_MP(info)* smp_info;
    process_t* current;
    int_frame_t temp_frame;
    int last_syscall;
} __attribute__((packed)) cpudata_t;


class CpuData {
public:
    static cpudata_t* Access();

};
