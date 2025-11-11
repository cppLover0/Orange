
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

#include <drivers/io.hpp>

#include <atomic>

using namespace arch::x86_64::cpu;

int balance_how_much_cpus = 1;

int how_much_cpus = 1;
std::atomic<int> temp_how_much_cpus[12];

locks::spinlock mp_lock;

void mp::sync(std::uint8_t id) {
    temp_how_much_cpus[id].fetch_add(1, std::memory_order_acq_rel);
    while (how_much_cpus != temp_how_much_cpus[id].load(std::memory_order_acquire)) {
        asm volatile("nop");
    }
    time::sleep(1000*1000); // perform 1 second sleep
    temp_how_much_cpus[id].store(0, std::memory_order_release);
} 

void __mp_bootstrap(struct LIMINE_MP(info)* smp_info) {
    mp_lock.lock();
    memory::paging::enablekernel();
    arch::x86_64::interrupts::idt::load();
    arch::x86_64::cpu::gdt::init();
    drivers::tsc::init();

    arch::x86_64::cpu::data()->smp.cpu_id = balance_how_much_cpus++;

    arch::x86_64::cpu::lapic::init(100);
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

    memset(temp_how_much_cpus,0,4*12);
    for(std::uint16_t i = 0;i < mp_info->cpu_count;i++) {
        if(mp_info->bsp_lapic_id != i) {
            mp_info->cpus[i]->goto_address = __mp_bootstrap;
        }
    }
}