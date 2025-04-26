
// SMP or MP idk source

#include <stdint.h>
#include <generic/mp/mp.hpp>
#include <generic/limineA/limineinfo.hpp>
#include <other/log.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/memory/paging.hpp>
#include <other/assembly.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <drivers/hpet/hpet.hpp>
#include <generic/memory/pmm.hpp>
#include <arch/x86_64/interrupts/syscalls/syscall.hpp>

uint64_t how_much_cpus = 0;
uint64_t temp_how_much_cpus = 0;

void __mp_bootstrap(struct LIMINE_MP(info)* smp_info) {
    
    __cli();
    __wrmsr(0xC0000101,0); // write to zero cuz idk what can be in this msr

    Paging::EnableKernel();

    GDT::Init();
    CpuData::Access()->smp_info = smp_info;
    IDT::Load();

    Syscall::Init();

    uint64_t stack = (uint64_t)PMM::VirtualBigAlloc(TSS_STACK_IN_PAGES); // for syscall

    Paging::alwaysMappedAdd(stack,TSS_STACK_IN_PAGES * PAGE_SIZE);

    Lapic::Init();
    Log("CPU %d is online !\n",smp_info->lapic_id);

    CpuData::Access()->kernel_stack = stack + (TSS_STACK_IN_PAGES * PAGE_SIZE);
    CpuData::Access()->user_stack = 0;

    Log("stack: 0x%p\n",stack);

    Log("Waiting for other CPUs...\n");
    MP::Sync();
    __sti();
    while(1) {
        __hlt();
    }

}

void MP::Init() {
    LimineInfo limine_info;
    how_much_cpus = limine_info.smp->cpu_count;
    temp_how_much_cpus = 0;
    for(uint64_t i = 0;i < limine_info.smp->cpu_count;i++) {
        if(i != limine_info.smp->bsp_lapic_id) {
            limine_info.smp->cpus[i]->goto_address = __mp_bootstrap; // in x86 it atomic soooo
        }
    }
    Log("CPUs count: %d\n",how_much_cpus);
}

void MP::Sync() {
    temp_how_much_cpus++;
    while(how_much_cpus != temp_how_much_cpus) {__nop();}
    HPET::Sleep(50000); // perform 50 ms sleep to sync
    temp_how_much_cpus = 0;
}