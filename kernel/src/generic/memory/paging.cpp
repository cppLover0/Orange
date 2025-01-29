
#include <stdint.h>
#include <generic/memory/pmm.hpp>
#include <generic/memory/paging.hpp>
#include <other/assembly.hpp>
#include <other/hhdm.hpp>
#include <generic/limine/limineinfo.hpp>
#include <lib/limine/limine.h>
#include <drivers/serial/serial.hpp>

uint64_t* __paging_next_level(uint64_t* table,uint64_t index) {
    if(!(table[index] & PTE_PRESENT))
        table[index] = (uint64_t)PMM::Alloc() | PTE_PRESENT | PTE_RW;
    return (uint64_t*)HHDM::toVirt(table[index] & PTE_MASK_VALUE);
}

uint64_t* kernel_cr3;

void* Paging::Map(uint64_t* cr3,uint64_t phys,uint64_t virt,uint64_t flags) {
    uint64_t aligned_phys = ALIGNPAGEDOWN(phys);
    uint64_t aligned_virt = ALIGNPAGEDOWN(virt); // yes hhdm always page aligned, but why not ?
    uint64_t* pml3 = __paging_next_level(cr3,PTE_INDEX(aligned_virt,39));
    uint64_t* pml2 = __paging_next_level(pml3,PTE_INDEX(aligned_virt,30));
    uint64_t* pml = __paging_next_level(pml2,PTE_INDEX(aligned_virt,21));
    pml[PTE_INDEX(aligned_virt,12)] = phys | flags;
    return (void*)virt;
}

void* Paging::HHDMMap(uint64_t* cr3,uint64_t phys,uint64_t flags) {
    return Map(cr3,phys,HHDM::toVirt(phys),flags);
}

void* Paging::KernelMap(uint64_t phys) {
    return Map(kernel_cr3,phys,HHDM::toVirt(phys),PTE_PRESENT | PTE_RW);
}

void Paging::Kernel(uint64_t* cr3) {
    LimineInfo c;
    extern uint64_t kernel_start;
    extern uint64_t kernel_end;
    uint64_t align_kernel_start = ALIGNPAGEDOWN((uint64_t)&kernel_start);
    uint64_t align_kernel_end = ALIGNPAGEUP((uint64_t)&kernel_end);
    for(uint64_t i = align_kernel_start;i < align_kernel_end;i+=PAGE_SIZE) {
        Paging::Map(cr3,i - c.ker_addr->virtual_base + c.ker_addr->physical_base,i,PTE_PRESENT | PTE_RW);
    }
}

void Paging::MemoryEntry(uint64_t* cr3,uint32_t type,uint64_t flags) {
    struct limine_memmap_entry* current_entry;
    LimineInfo b;
    for(uint64_t i = 0;i < b.memmap->entry_count;i++) {
        current_entry = b.memmap->entries[i];
        if(current_entry->type == type) {
             for(uint64_t i = current_entry->base;i < current_entry->base + current_entry->length;i+=PAGE_SIZE) {
                HHDMMap(cr3,i,flags);
            }
        }
    }
}

void Paging::Pat(uint8_t index, uint8_t type) {
    uint64_t pat = __rdmsr(0x277);
    pat &= ~(0xFFULL << (index * 8));
    pat |= ((uint64_t)type << (index * 8));
    __wrmsr(0x277, pat);
}

void Paging::EnablePaging(uint64_t* virt_cr3) {
    asm volatile("mov %0, %%cr3" : : "r" (HHDM::toPhys((uint64_t)virt_cr3)) : "memory");
}

void Paging::EnableKernel() {
    EnablePaging(kernel_cr3);
}

void Paging::Init() {
    Pat(1,1);
    kernel_cr3 = (uint64_t*)PMM::VirtualAlloc();
    Serial::printf("Mapping usable memory\n");
    MemoryEntry(kernel_cr3,LIMINE_MEMMAP_USABLE,PTE_PRESENT | PTE_RW);
    Serial::printf("Mapping kernel\n");
    Kernel(kernel_cr3);
    Serial::printf("Mapping framebuffer\n");
    MemoryEntry(kernel_cr3,LIMINE_MEMMAP_FRAMEBUFFER,PTE_PRESENT | PTE_RW | PTE_WC);
    Serial::printf("Mapping bootloader reclaimable\n");
    MemoryEntry(kernel_cr3,LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE,PTE_PRESENT | PTE_RW);
    Serial::printf("Activating paging\n");
    EnableKernel();
}