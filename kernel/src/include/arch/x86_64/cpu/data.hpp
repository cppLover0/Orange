
#pragma once

#include <lib/limineA/limine.h>
#include <arch/x86_64/cpu/gdt.hpp>

typedef struct {
    GDT gdt;
    struct LIMINE_MP(info)* smp_info;
} __attribute__((packed)) cpudata_t;


class CpuData {
public:
    static cpudata_t* Access();

};
