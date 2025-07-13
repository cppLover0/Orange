
#include <cstdint>
#include <generic/mm/pmm.hpp>
#include <generic/mm/paging.hpp>
#include <generic/mm/heap.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <etc/libc.hpp>
#include <config.hpp>

gdt_t original_gdt = {
    {0,0,0,0,0,0}, /* 0x0 Null */
    {0,0,0,0x9a,0xa2,0}, /* 0x08 64 bit code */
    {0,0,0,0x92,0xa0,0}, /* just align user data and user code */
    {0,0,0,0xF2,0,0}, /* 0x18 user data, here because its easier for syscalls */
    {0,0,0,0xFA,0x20,0}, /* 0x20 user code */
    {0x68,0,0,0x89,0x20,0,0,0} /* 0x28 tss */
};

void arch::x86_64::cpu::gdt::init() {
    arch::x86_64::cpu::gdt* new_gdt = (arch::x86_64::cpu::gdt*)memory::heap::malloc(sizeof(arch::x86_64::cpu::gdt));
    tss_t* tss = new tss_t;
    
    /* For optimizations kheap doesn't zero memory */
    memset(&new_gdt->gdt_obj,0,sizeof(gdt_t));
    memset(tss,0,sizeof(tss_t));

    memcpy(&new_gdt->gdt_obj,&original_gdt,sizeof(gdt_t));

    tss->rsp[0] = memory::pmm::helper::alloc_kernel_stack(KERNEL_STACK_SIZE);
    tss->ist[0] = memory::pmm::helper::alloc_kernel_stack(KERNEL_STACK_SIZE); /* Exceptions */
    tss->ist[1] = memory::pmm::helper::alloc_kernel_stack(KERNEL_STACK_SIZE); /* Timer */
    tss->ist[2] = memory::pmm::helper::alloc_kernel_stack(KERNEL_STACK_SIZE); /* IRQ Layout */

    tss->iopb_offsset = sizeof(tss_t);
    new_gdt->gdt_obj.tss.baselow16 = (std::uint64_t)tss & 0xFFFF;
    new_gdt->gdt_obj.tss.basemid8 = ((std::uint64_t)tss >> 16) & 0xFF;
    new_gdt->gdt_obj.tss.basehigh8 = ((std::uint64_t)tss >> 24) & 0xFF;
    new_gdt->gdt_obj.tss.baseup32 = (std::uint64_t)tss >> 32;

    new_gdt->gdtr.size = sizeof(gdt_t) - 1;
    new_gdt->gdtr.base = (std::uint64_t)&new_gdt->gdt_obj;

    new_gdt->LoadGDT();
    new_gdt->LoadTSS();

    /* Also i want to init cpu data too */
    auto cpudata = arch::x86_64::cpu::data();
    cpudata->kernel_stack = memory::pmm::helper::alloc_kernel_stack(KERNEL_STACK_SIZE);

}