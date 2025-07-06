
#pragma once

#include <lib/limineA/limine.h>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <stdint.h>

typedef struct {
    uint64_t user_stack;
    uint64_t kernel_stack;
    uint64_t kernel_cr3;
    uint64_t reserved;
    uint64_t user_cr3;
    GDT gdt;
    struct LIMINE_MP(info)* smp_info;
    process_t* current;
    process_queue_run_list_t* next;
    int_frame_t temp_frame;
    char* current_fd; // used for advanced writings (devfs ring buffer)

    char is_advanced_access;
    uint64_t offset;
    uint64_t* count;

    int last_syscall;
} __attribute__((packed)) cpudata_t;


class CpuData {
public:
    static cpudata_t* Access();

};
