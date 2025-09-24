
#include <drivers/tsc.hpp>
#include <drivers/hpet.hpp>
#include <cstdint>

#include <generic/time.hpp>

#include <arch/x86_64/cpu/data.hpp>

#include <etc/logging.hpp>

#include <etc/assembly.hpp>

using namespace drivers;

std::uint64_t freq;

void tsc::init() {
    std::uint64_t start = __rdtsc();
    time::sleep(100);
    std::uint64_t end = __rdtsc();
    std::uint64_t d = end - start;
    arch::x86_64::cpu::data()->tsc.freq = (d * 1000000ULL) / (100 * 1000ULL);
}


void tsc::sleep(std::uint64_t us) {
    
    if(arch::x86_64::cpu::data()->tsc.freq == 0) {
        drivers::hpet::sleep(us);
        return;
    }

    std::uint64_t current = currentnano();
    std::uint64_t end = us * 1000;
    while((currentnano() - current) < end);
        __nop();
}

std::uint64_t tsc::currentnano() {
    return (__rdtsc() * 1000000ULL) / arch::x86_64::cpu::data()->tsc.freq;
}

std::uint64_t tsc::currentus() {
    return currentnano() / 1000;
}