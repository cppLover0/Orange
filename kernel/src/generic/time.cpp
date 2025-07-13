
#include <generic/time.hpp>
#include <cstdint>

#include <drivers/tsc.hpp>
#include <drivers/kvmtimer.hpp>
#include <drivers/hpet.hpp>

std::uint64_t time::counter() {
    extern std::uint16_t KERNEL_GOOD_TIMER;
    switch(KERNEL_GOOD_TIMER) {
        case TSC_TIMER: 
           return drivers::tsc::currentnano();
        case KVM_TIMER:
            return drivers::kvmclock::currentnano();
    }
}

void time::sleep(std::uint64_t us) {
    extern std::uint16_t KERNEL_GOOD_TIMER;
    switch(KERNEL_GOOD_TIMER) {
        case TSC_TIMER: 
            drivers::tsc::sleep(us);
            break;
        case KVM_TIMER:
            drivers::kvmclock::sleep(us);
            break;
    }
    return;
} 