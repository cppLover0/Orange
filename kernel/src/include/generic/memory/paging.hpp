
// i'll use hhdm 

#pragma once

#include <stdint.h>
#include <config.hpp>

#define PTE_PRESENT (1 << 0)
#define PTE_RW (1 << 1)
#define PTE_USER (1 << 2)
#define PTE_WC (1 << 3)

#define ROUNDUP(VALUE,ROUND) ((VALUE + (ROUND - 1)) / ROUND)
#define ALIGNPAGEUP(VALUE) (ROUNDUP(VALUE,PAGE_SIZE) * PAGE_SIZE)
#define ALIGNPAGEDOWN(VALUE) ((VALUE / PAGE_SIZE) * PAGE_SIZE)
#define PTE_MASK_VALUE 0x000ffffffffff000

#define PTE_INDEX(address,bit) ((address & (uint64_t)0x1FF << bit) >> bit)

typedef struct alwaysMapped {
    uint64_t base;
    uint64_t size;
    char is_phys;
    struct alwaysMapped* next;
} alwaysMapped_t;

class Paging {
public:
    static void Init();

    static void* Map(uint64_t* cr3,uint64_t phys,uint64_t virt,uint64_t flags);

    static void* HHDMMap(uint64_t* cr3,uint64_t phys,uint64_t flags);

    static void* KernelMap(uint64_t phys);

    static void Kernel(uint64_t* cr3);

    static void MemoryEntry(uint64_t* cr3,uint32_t type,uint64_t flags);

    static void Pat(uint8_t index, uint8_t type);

    static void alwaysMappedMap(uint64_t* cr3);

    static void alwaysMappedAdd(uint64_t base,uint64_t size);

    static void EnablePaging(uint64_t* virt_cr3);

    static void EnableKernel();

};