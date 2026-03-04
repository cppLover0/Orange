#include <cstdint>
#include <generic/time.hpp>
#include <arch/aarch64/cpu/timer.hpp>
#include <generic/arch.hpp>
#include <klibc/stdio.hpp>

void aarch64::timer::init() {
    aarch64::timer::aarch64_timer* aarch64_timer = new aarch64::timer::aarch64_timer;
    time::setup_timer(aarch64_timer);
    klibc::printf("AArch64 timer: Set aarch64 timer as default\r\n");
}

namespace aarch64 {
    int timer::aarch64_timer::get_priority() {
        return 999999;
    }

    std::uint64_t timer::aarch64_timer::current_nano() {
        std::uint64_t cntpct_el0 = 0;
        std::uint64_t cntfrq_el0 = 0;
        asm volatile("mrs %0, CNTPCT_EL0" : "=r"(cntpct_el0));
        asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(cntfrq_el0));
        constexpr std::uint64_t NANOSECONDS_PER_SECOND = 1000000000ULL;
    
        #ifdef __SIZEOF_INT128__
        __uint128_t temp = static_cast<__uint128_t>(cntpct_el0) * NANOSECONDS_PER_SECOND;
        return static_cast<std::uint64_t>(temp / cntfrq_el0);
        #else
        std::uint64_t whole_seconds = cntpct_el0 / cntfrq_el0;
        std::uint64_t remainder_ticks = cntpct_el0 % cntfrq_el0;
        
        std::uint64_t nanos_from_whole = whole_seconds * NANOSECONDS_PER_SECOND;
        std::uint64_t nanos_from_remainder = (remainder_ticks * NANOSECONDS_PER_SECOND) / cntfrq_el0;
        
        return nanos_from_whole + nanos_from_remainder;
        #endif
    }

    void timer::aarch64_timer::sleep(std::uint64_t us) {
        std::uint64_t current = this->current_nano();
        std::uint64_t end_ns = us * 1000;
        std::uint64_t target = current + end_ns;
        
        while(this->current_nano() < target) {
            arch::pause();
        }
    }

};