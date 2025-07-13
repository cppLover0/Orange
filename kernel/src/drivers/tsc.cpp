
#include <drivers/tsc.hpp>
#include <arch/x86_64/interrupts/pit.hpp>
#include <cstdint>

#include <etc/logging.hpp>

#include <etc/assembly.hpp>

using namespace drivers;

std::uint64_t freq = 0;

void tsc::init() {
    std::uint64_t start = __rdtsc();
    arch::x86_64::interrupts::pit::sleep(100 * 1000);
    std::uint64_t end = __rdtsc();
    std::uint64_t d = end - start;
    freq = (d * 1000000000ULL) / (100 * 1000000ULL);
}

void tsc::sleep(std::uint64_t us) {
    std::uint64_t current = currentnano();
    std::uint64_t end = us * 1000;
    while((currentnano() - current) < end);
        __nop();
}

std::uint64_t tsc::currentnano() {
    return (__rdtsc() * 1000000000ULL) / freq;
}