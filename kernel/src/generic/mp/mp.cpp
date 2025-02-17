
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

uint64_t how_much_cpus = 0;
uint64_t temp_how_much_cpus = 0;

void __mp_bootstrap(struct LIMINE_MP(info)* smp_info) {
    
    __cli();
    Paging::EnableKernel();

    GDT::Init();
    CpuData::Access()->smp_info = smp_info;
    IDT::Load();

    Lapic::Init();
    Log("CPU %d is online !\n",smp_info->lapic_id);

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
    HPET::Sleep(1000); // perform 1 ms sleep to sync
    temp_how_much_cpus = 0;
}