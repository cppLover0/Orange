#include <generic/bootloader/bootloader.hpp>
#include <generic/lock/spinlock.hpp>
#if defined(__x86_64__)
#include <arch/x86_64/cpu_local.hpp>
#endif
#include <generic/time.hpp>
#include <generic/arch.hpp>
#include <generic/mp.hpp>
#include <klibc/stdio.hpp>
#include <cstdint>
#include <atomic>

std::uint32_t balance_how_much_cpus = 1;
std::uint32_t how_much_cpus = 0;
static mp::barrier mp_barrier;
locks::spinlock smp_lock;

std::uint32_t mp::cpu_count() {
    return how_much_cpus;
}

void mp::sync() {
    if (how_much_cpus <= 1) return;

    uint32_t current_gen = mp_barrier.generation.load(std::memory_order_acquire);
    if (mp_barrier.count.fetch_add(1, std::memory_order_acq_rel) == (how_much_cpus - 1)) {
        mp_barrier.count.store(0, std::memory_order_relaxed); 
        mp_barrier.generation.fetch_add(1, std::memory_order_release); 
    } else {
        while (mp_barrier.generation.load(std::memory_order_acquire) == current_gen) {
            arch::pause();
        }
    }
}

void smptrampoline(limine_mp_info* smp_info) {
    
    smp_lock.lock();
    std::uint32_t enum_cpu = smp_info->processor_id;
    arch::init(ARCH_INIT_MP);
#if defined(__x86_64__)
    x86_64::cpu_data()->cpu = balance_how_much_cpus++;
    enum_cpu = x86_64::cpu_data()->cpu;
#endif
    log("smp", "cpu %d is online (%d)",smp_info->lapic_id,enum_cpu);
    smp_lock.unlock();
    mp::sync();
    if(time::timer) time::timer->sleep(10000);
    mp::sync();
    arch::enable_interrupts();
    while(true) {
        arch::wait_for_interrupt();
    }
}

void mp::init() {
    limine_mp_response* mp_info = bootloader::bootloader->get_mp_info();
    if(!mp_info) {
        how_much_cpus = 1;
        return;
    }
        
    how_much_cpus = mp_info->cpu_count;
    for(std::uint16_t i = 0;i < mp_info->cpu_count;i++) {
        if(mp_info->bsp_lapic_id != i) {
            mp_info->cpus[i]->goto_address = smptrampoline;
        }
    }
    log("mp", "detected %d cpus", mp_info->cpu_count);
}