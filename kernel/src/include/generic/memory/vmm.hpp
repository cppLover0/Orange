
#include <stdint.h>
#include <arch/x86_64/scheduling/scheduling.hpp>

#pragma once

typedef struct vmm_obj {
    uint64_t base;
    uint64_t phys;
    uint64_t len;
    uint64_t flags;

    uint64_t src_len;

    struct vmm_obj* next;
} __attribute__((packed)) vmm_obj_t;

class VMM {
public:
    static void Init(process_t* proc);
    static void* Alloc(process_t* proc,uint64_t length,uint64_t flags);
    static void* Map(process_t* proc,uint64_t base,uint64_t length,uint64_t flags);
    static void* CustomAlloc(process_t* proc,uint64_t virt,uint64_t length,uint64_t flags);
    static void* Mark(process_t* proc,uint64_t base,uint64_t phys,uint64_t length,uint64_t flags); // mark some regions as already used
    static void Clone(process_t* dest_proc,process_t* src_proc);
    static void Modify(process_t* proc,uint64_t dest_base,uint64_t new_phys);
    static vmm_obj_t* Get(process_t* proc,uint64_t base);

    static void Free(process_t* proc);

    static void Reload(process_t* proc);

    static void Dump(process_t* proc);

};