
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

#include <atomic>

using namespace arch::x86_64::cpu;

int how_much_cpus;
std::atomic<int> temp_how_much_cpus[12];

locks::spinlock mp_lock;

void mp::sync(std::uint8_t id) {
    temp_how_much_cpus[id].fetch_add(1, std::memory_order_acq_rel);
    while (how_much_cpus != temp_how_much_cpus[id].load(std::memory_order_acquire)) {
        asm volatile("nop");
    }
    time::sleep(50000); // perform 50 ms sleep to sync
    temp_how_much_cpus[id].store(0, std::memory_order_release);
} 

extern int how_much_cpus;

void __mp_bootstrap(struct LIMINE_MP(info)* smp_info) {
    mp_lock.lock();
    memory::paging::enablekernel();
    arch::x86_64::interrupts::idt::load();
    arch::x86_64::cpu::gdt::init();
    drivers::tsc::init();

    std::uint64_t start = drivers::tsc::currentnano();
    for(int i = 0; i < 1000; i++) {
        asm volatile("pause");
    }
    std::uint64_t end = drivers::tsc::currentnano();

    std::uint64_t delta = (end - start) / 1000; 

    arch::x86_64::cpu::lapic::init(delta);
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

    if(how_much_cpus == 0)
        how_much_cpus = 1;
        
    memset(temp_how_much_cpus,0,4*12);
    for(std::uint16_t i = 0;i < mp_info->cpu_count;i++) {
        if(mp_info->bsp_lapic_id != i) {
            mp_info->cpus[i]->goto_address = __mp_bootstrap;
        }
    }
}