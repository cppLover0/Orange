
#include <cstdint>

#include <arch/x86_64/cpu/smp.hpp>
#include <generic/time.hpp>

#include <arch/x86_64/cpu/data.hpp>

#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/mm/paging.hpp>

#include <generic/locks/spinlock.hpp>
#include <etc/bootloaderinfo.hpp>

#include <drivers/tsc.hpp>

#include <arch/x86_64/syscalls/syscalls.hpp>

#include <arch/x86_64/cpu/sse.hpp>

#include <etc/logging.hpp>

#include <etc/libc.hpp>

#include <limine.h>

using namespace arch::x86_64::cpu;

std::uint64_t how_much_cpus = 0;
std::uint64_t temp_how_much_cpus[12];

locks::spinlock mp_lock;

void mp::sync(std::uint8_t id) {
    temp_how_much_cpus[id]++;
    while(how_much_cpus != temp_how_much_cpus[id]) {asm volatile("pause");}
    time::sleep(500000); // perform 500 ms sleep to sync
    temp_how_much_cpus[id] = 0;
} 

void __mp_bootstrap(struct LIMINE_MP(info)* smp_info) {
    mp_lock.lock();
    memory::paging::enablekernel();
    arch::x86_64::interrupts::idt::load();
    arch::x86_64::cpu::gdt::init();
    drivers::tsc::init();
    arch::x86_64::cpu::lapic::init(1000);
    arch::x86_64::cpu::data()->smp.cpu_id = smp_info->processor_id;
    arch::x86_64::cpu::sse::init();
    arch::x86_64::syscall::init();
    Log::Display(LEVEL_MESSAGE_OK,"Cpu %d is online \n",arch::x86_64::cpu::data()->smp.cpu_id);
    mp_lock.unlock();
    mp::sync(0);
    mp::sync(1);
    asm volatile("sti");
    while(1) {
        asm volatile("hlt");
    }
}

void mp::init() {
    struct LIMINE_MP(response)* mp_info = BootloaderInfo::AccessMP();
    how_much_cpus = mp_info->cpu_count;
    memset(temp_how_much_cpus,0,8*12);
    for(std::uint16_t i = 0;i < mp_info->cpu_count;i++) {
        if(mp_info->bsp_lapic_id != i) {
            mp_info->cpus[i]->goto_address = __mp_bootstrap;
        }
    }
}