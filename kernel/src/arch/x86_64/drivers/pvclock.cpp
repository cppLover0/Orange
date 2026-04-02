#define BILLION 1000000000UL
#include <cstdint>
#include <drivers/acpi.hpp>
#include <arch/x86_64/drivers/pvclock.hpp>
#include <arch/x86_64/cpu_local.hpp>
#include <utils/gobject.hpp>
#include <klibc/stdio.hpp>
#include <arch/x86_64/assembly.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>

#include <generic/arch.hpp>

void drivers::pvclock::init() {
    std::uint32_t a,b,c,d;

    assembly::cpuid(0x40000000,0,&a,&b,&c,&d);
    if(b != 0x4b4d564b || d != 0x4d || c != 0x564b4d56)
        return;

    assembly::cpuid(0x40000001,0,&a,&b,&c,&d);
    if(!(a & (1 << 3)))
        return;

    x86_64::cpu_data()->pvclock_buffer = (void*)(pmm::freelist::alloc_4k() + etc::hhdm());

    assembly::wrmsr(0x4b564d01,((std::uint64_t)x86_64::cpu_data()->pvclock_buffer - etc::hhdm()) | 1);
}

void drivers::pvclock::bsp_init() {
    std::uint32_t a,b,c,d;

    assembly::cpuid(0x40000000,0,&a,&b,&c,&d);
    if(b != 0x4b4d564b || d != 0x4d || c != 0x564b4d56)
        return;

    assembly::cpuid(0x40000001,0,&a,&b,&c,&d);
    if(!(a & (1 << 3)))
        return;

    pvclock::init();

    drivers::pvclock_timer* timer = new drivers::pvclock_timer;
    time::setup_timer(timer);

    log("pvclock", "pointer to buffer 0x%p",x86_64::cpu_data()->pvclock_buffer);
}

locks::preempt_spinlock pvclock_lock;

namespace drivers {
    
    std::uint64_t pvclock_timer::current_nano() {
        bool state = pvclock_lock.lock();
        auto* info = (pvclock_vcpu_time_info*)CPU_LOCAL_READ(pvclock_buffer);
        uint32_t version;
        uint64_t time0;

        do {
            version = info->version;
            std::atomic_thread_fence(std::memory_order_acquire);

            uint32_t lo, hi;
            asm volatile ("rdtsc" : "=a"(lo), "=d"(hi) : : "memory");
            uint64_t tsc_v = (static_cast<uint64_t>(hi) << 32) | lo;

            uint64_t delta = tsc_v - info->tsc_timestamp;
            
            if (info->tsc_shift >= 0)
                delta <<= info->tsc_shift;
            else
                delta >>= -info->tsc_shift;

            uint64_t scaled_delta = (static_cast<unsigned __int128>(delta) * info->tsc_to_system_mul) >> 32;
            
            time0 = info->system_time + scaled_delta;

            std::atomic_thread_fence(std::memory_order_acquire);
        } while ((info->version & 1) || (info->version != version));

        pvclock_lock.unlock(state);
        return time0;
    }


    void pvclock_timer::sleep(std::uint64_t us) {
        std::uint64_t current = this->current_nano();
        std::uint64_t end_ns = us * 1000;
        std::uint64_t target = current + end_ns;
        
        while(this->current_nano() < target) {
            arch::pause();
        }
    }

    int pvclock_timer::get_priority() {
        return 999999999; 
    }
}