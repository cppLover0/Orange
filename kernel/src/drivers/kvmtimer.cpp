
#include <cstdint>

#include <drivers/kvmtimer.hpp>
#include <generic/time.hpp>
#include <etc/assembly.hpp>
#include <etc/etc.hpp>

#include <etc/logging.hpp>

#include <generic/mm/pmm.hpp>

struct pvclock_vcpu_time_info* kvmclock_info;

extern std::uint16_t KERNEL_GOOD_TIMER;

void drivers::kvmclock::init() {

    std::uint32_t a,b,c,d;

    __cpuid(0x40000000,0,&a,&b,&c,&d);
    if(b != 0x4b4d564b || d != 0x4d || c != 0x564b4d56)
        return;

    __cpuid(0x40000001,0,&a,&b,&c,&d);
    if(!(a & (1 << 3)))
        return;

    KERNEL_GOOD_TIMER = KVM_TIMER;

    kvmclock_info = (struct pvclock_vcpu_time_info*)memory::pmm::_virtual::alloc(4096);

    __wrmsr(0x4b564d01,(std::uint64_t)Other::toPhys(kvmclock_info) | 1);
    Log::Display(LEVEL_MESSAGE_OK,"KVMClock initializied\n");

}

void drivers::kvmclock::sleep(std::uint64_t us) {
    std::uint64_t start = currentnano();
    std::uint64_t conv = us * 1000;
    while((currentnano() - start) < conv)
        asm volatile("nop");
}

std::uint64_t drivers::kvmclock::currentnano() {
    std::uint64_t time0 = __rdtsc() - kvmclock_info->tsc_timestamp;
    if(kvmclock_info->tsc_shift >= 0)
        time0 <<= kvmclock_info->tsc_shift;
    else
        time0 >>= -kvmclock_info->tsc_shift;
    time0 = (time0 * kvmclock_info->tsc_to_system_mul) >> 32;
    time0 = time0 + kvmclock_info->system_time;
    return time0;
}