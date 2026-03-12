#include <cstdint>
#include <generic/heap.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <klibc/stdio.hpp>
#include <arch/x86_64/cpu_local.hpp>

#define KERNEL_STACK_SIZE 32 * 1024

x86_64::gdt::gdt_t original_gdt = {
    {0,0,0,0,0,0}, /* 0x0 Null */
    {0,0,0,0x9a,0xa2,0}, /* 0x08 64 bit code */
    {0,0,0,0x92,0xa0,0}, /* just align user data and user code */
    {0,0,0,0xF2,0,0}, /* 0x18 user data, here because its easier for syscalls */
    {0,0,0,0xFA,0x20,0}, /* 0x20 user code */
    {0x68,0,0,0x89,0x20,0,0,0} /* 0x28 tss */
};

void x86_64::gdt::init() {
    gdt_t* new_gdt = new gdt_t;
    tss_t* tss = new tss_t;

    gdt_pointer_t* gdtr = new gdt_pointer_t;
    klibc::memcpy(new_gdt,&original_gdt,sizeof(gdt_t));

    tss->rsp[0] = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm());
    tss->ist[0] = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm()); /* Exceptions */
    tss->ist[1] = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm()); /* Timer */
    tss->ist[2] = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm()); /* IRQ Layout */
    tss->ist[3] = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm()); /* For ignorestub */

    tss->iopb_offsset = sizeof(tss_t);
    new_gdt->tss.baselow16 = (std::uint64_t)tss & 0xFFFF;
    new_gdt->tss.basemid8 = ((std::uint64_t)tss >> 16) & 0xFF;
    new_gdt->tss.basehigh8 = ((std::uint64_t)tss >> 24) & 0xFF;
    new_gdt->tss.baseup32 = (std::uint64_t)tss >> 32;

    gdtr->size = sizeof(gdt_t) - 1;
    gdtr->base = (std::uint64_t)new_gdt;

    load_gdt(gdtr);
    load_tss();

    x86_64::restore_cpu_data();
    auto cpudata = x86_64::cpu_data();
    cpudata->timer_ist_stack = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm());

    static bool is_print = 0;
    if(!is_print) { klibc::printf("GDT: tss->rsp0 0x%p tss->ist0 0x%p\r\n",tss->rsp[0],tss->ist[0],tss->ist[1],tss->ist[2],tss->ist[3],cpudata->timer_ist_stack); is_print = 1; }

}