
// i'll use hhdm 

#pragma once

#include <stdint.h>
#include <config.hpp>

#define PTE_PRESENT (1ull << 0)
#define PTE_RW (1ull << 1)
#define PTE_USER (1ull << 2)
#define PTE_WC (1ull << 3)

#define ROUNDUP(VALUE,ROUND) ((VALUE + (ROUND - 1)) / ROUND)
#define ALIGNPAGEUP(VALUE) (ROUNDUP(VALUE,PAGE_SIZE) * PAGE_SIZE)
#define ALIGNPAGEDOWN(VALUE) ((VALUE / PAGE_SIZE) * PAGE_SIZE)

#define CROUNDUP(VALUE,ROUND) ((VALUE + (ROUND - 1)) / ROUND)
#define CALIGNPAGEUP(VALUE,c) (ROUNDUP(VALUE,c) * c)
#define CALIGNPAGEDOWN(VALUE,c) ((VALUE / c) * c)

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

    static uint64_t PhysFromVirt(uint64_t* cr3,uint64_t virt);

    static void Unmap(uint64_t* cr3, uint64_t virt, uint64_t size_in_pages);

    static uint64_t* KernelGet();

    static void Copy(uint64_t* dest_cr3, uint64_t* src_cr3);

};