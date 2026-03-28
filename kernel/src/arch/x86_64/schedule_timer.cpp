#include <arch/x86_64/schedule_timer.hpp>
#include <arch/x86_64/cpu/idt.hpp>
#include <arch/x86_64/cpu/xapic.hpp>
#include <arch/x86_64/cpu_local.hpp>
#include <generic/scheduling.hpp>
#include <klibc/stdio.hpp>
#include <generic/mp.hpp>
#include <generic/time.hpp>
#include <generic/lock/spinlock.hpp>
#include <atomic>

extern "C" void scheduler_timer_asm();

int is_timer_off = 0;
std::atomic<int> stfu_cpus = 0;

extern "C" void timer_tick(x86_64::idt::int_frame_t* ctx) {

    if(is_timer_off) {
        stfu_cpus++;
        arch::hcf();
    }

    process::schedule((void*)ctx);
    static std::atomic<int> i = 0;
    klibc::printf("timer tick %lli %d\r", i++,x86_64::cpu_data()->cpu);
    x86_64::apic::eoi();
}

void x86_64::schedule_timer::off() {
    is_timer_off = 1;

    if(mp::cpu_count() <= 1)
        return;

    std::uint32_t timeout = 10;

    log("poweroff", "waiting for all cpus to done work"); // used from poweroff so
    while(stfu_cpus != (std::int32_t)(mp::cpu_count() - 1)) {
        arch::memory_barrier();

        if(--timeout == 0) {
            log("poweroff", "poweroff: Can't wait longer, forcing them to be disabled");
            x86_64::lapic::off();
            return;
        }

        if(time::timer) {
            time::timer->sleep(50000);
        } else {
            arch::pause();
        }
    }
}

void x86_64::schedule_timer::init() {
    x86_64::idt::set_entry((std::uint64_t)scheduler_timer_asm,32, 0x8E, 2);
}