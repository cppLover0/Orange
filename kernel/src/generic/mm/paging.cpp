
#include <cstdint>
#include <generic/mm/pmm.hpp>
#include <generic/mm/heap.hpp>
#include <generic/mm/paging.hpp>
#include <etc/bootloaderinfo.hpp>
#include <etc/logging.hpp>
#include <limine.h>
#include <etc/etc.hpp>

static void init();
std::uint64_t kernel_cr3;
alwaysmapped_t* alwmap = 0;

uint64_t* __paging_next_level(std::uint64_t* table,std::uint16_t idx,std::uint64_t flags,std::uint32_t id) {
    if(!(table[idx] & PTE_PRESENT))
        table[idx] = memory::pmm::_physical::allocid(4096,id) | flags;
    return (uint64_t*)Other::toVirt(table[idx] & PTE_MASK_VALUE);
}

void memory::paging::map(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t flags) {
    std::uint64_t align_phys = ALIGNDOWN(phys,4096);
    std::uint64_t align_virt = ALIGNDOWN(virt,4096);
    std::uint64_t* cr30 = (std::uint64_t*)Other::toVirt(cr3);
    std::uint64_t new_flags = flags;
    if(PTE_INDEX(align_virt,39) < 256)
        new_flags |= PTE_USER;
    uint64_t* pml3 = __paging_next_level(cr30,PTE_INDEX(align_virt,39),new_flags,0);
    uint64_t* pml2 = __paging_next_level(pml3,PTE_INDEX(align_virt,30),new_flags,0);
    uint64_t* pml = __paging_next_level(pml2,PTE_INDEX(align_virt,21),new_flags,0);
    pml[PTE_INDEX(align_virt,12)] = align_phys | flags;
}

void memory::paging::mapid(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t flags,std::uint32_t id) {
    std::uint64_t align_phys = ALIGNDOWN(phys,4096);
    std::uint64_t align_virt = ALIGNDOWN(virt,4096);
    std::uint64_t* cr30 = (std::uint64_t*)Other::toVirt(cr3);
    std::uint64_t new_flags = PTE_PRESENT | PTE_RW;
    if(PTE_INDEX(align_virt,39) < 256)
        new_flags |= PTE_USER;
    uint64_t* pml3 = __paging_next_level(cr30,PTE_INDEX(align_virt,39),new_flags,id);
    uint64_t* pml2 = __paging_next_level(pml3,PTE_INDEX(align_virt,30),new_flags,id);
    uint64_t* pml = __paging_next_level(pml2,PTE_INDEX(align_virt,21),new_flags,id);
    pml[PTE_INDEX(align_virt,12)] = align_phys | flags;
}

void memory::paging::maprange(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t len,std::uint64_t flags) {
    for(std::uint64_t i = 0; i <= len; i += 4096) {
        map(cr3,phys + i,virt + i,flags);
    }
}

void memory::paging::maprangeid(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t len,std::uint64_t flags, std::uint32_t id) {
    for(std::uint64_t i = 0; i <= len; i += 4096) {
        mapid(cr3,phys + i,virt + i,flags,id);
    }
}

void memory::paging::mapentry(std::uint64_t cr3,std::uint8_t type,std::uint64_t add_flags) {
    limine_memmap_response* mmap = BootloaderInfo::AccessMemoryMap();
    limine_memmap_entry* current = mmap->entries[0];
    for(int i = 0;i < mmap->entry_count;i++) {
        current = mmap->entries[i];
        if(current->type == type)
            maprange(cr3,current->base,(std::uint64_t)Other::toVirt(current->base),current->length,PTE_PRESENT | PTE_RW | add_flags);
    }
}

void* memory::paging::kernelmap(std::uint64_t cr3,std::uint64_t phys) {
    map(kernel_cr3,phys,(std::uint64_t)Other::toVirt(phys),PTE_PRESENT | PTE_RW);
    return Other::toVirt(phys);
}

void __paging_map_kernel(std::uint64_t cr3,std::uint32_t id) {
    extern std::uint64_t kernel_start;
    extern std::uint64_t kernel_end;
    limine_executable_address_response* ker = BootloaderInfo::AccessKernel();
    
    for(std::uint64_t i = ALIGNDOWN((std::uint64_t)&kernel_start,4096);i < ALIGNUP((std::uint64_t)&kernel_end,4096);i += 4096) {
        memory::paging::mapid(cr3,i - ker->virtual_base + ker->physical_base,i,PTE_PRESENT | PTE_RW,0);
    }
}

void memory::paging::mapkernel(std::uint64_t cr3,std::uint32_t id) {
    __paging_map_kernel(cr3,id);
}

void memory::paging::enablekernel() {
    asm volatile("mov %0, %%cr3" : : "r"(kernel_cr3) : "memory");
}

void memory::paging::enablepaging(std::uint64_t cr3) {
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

std::uint64_t memory::paging::kernelget() {
    return kernel_cr3;
}

void __map_range_id(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t len,std::uint64_t flags,std::uint32_t id) {
    for(std::uint64_t i = 0; i < len; i += 4096) {
        memory::paging::map(cr3,phys + i,virt + i,flags);
    }
}

void memory::paging::alwaysmappedadd(std::uint64_t phys, std::uint64_t len) {
    alwaysmapped_t* alw = new alwaysmapped_t;
    alw->next = alwmap;
    alw->phys = phys;
    alw->len = len;
    alwmap = alw;
}

void memory::paging::alwaysmappedmap(std::uint64_t cr3,std::uint32_t id) {
    alwaysmapped_t* current = alwmap;
    while(current) {
        __map_range_id(cr3,current->phys,(std::uint64_t)Other::toVirt(current->phys),current->len,PTE_RW | PTE_PRESENT,id);  
        current = current->next;
    }
}

void memory::paging::init() {
    kernel_cr3 = memory::pmm::_physical::alloc(4096);
    memory::pat(1,1);
    memory::pat(2,0);
    mapentry(kernel_cr3,LIMINE_MEMMAP_USABLE,0);
    mapentry(kernel_cr3,LIMINE_MEMMAP_FRAMEBUFFER,PTE_WC);
    mapentry(kernel_cr3,LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE,0);
    mapentry(kernel_cr3,LIMINE_MEMMAP_EXECUTABLE_AND_MODULES,0);
    mapkernel(kernel_cr3,0);
    enablekernel();
}