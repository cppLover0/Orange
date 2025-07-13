
#include <cstdint>
#include <arch/x86_64/interrupts/pic.hpp>
#include <arch/x86_64/interrupts/irq.hpp>
#include <arch/x86_64/interrupts/pit.hpp>
#include <drivers/hpet.hpp>

using namespace arch::x86_64::interrupts;

std::uint64_t pit_counter = 0;

int __pit_int_handler(void *arg) {
    pit_counter++;
    return 0;
}

void arch::x86_64::interrupts::pit::init() {
    drivers::io io;
    std::uint8_t entry = irq::create(0,IRQ_TYPE_LEGACY,__pit_int_handler,0,0);
    std::uint32_t div = PIT_FREQUENCY / 1000;
    io.outb(0x43, 0x36);
    io.outb(0x40,div & 0xFF);
    io.outb(0x40,(div >> 8) & 0xFF);

    pic::clear(0);

}

/* PIT Sleep is broken, redirect to hpet */
void arch::x86_64::interrupts::pit::sleep(std::uint32_t ms) {
    drivers::hpet::sleep(ms);
}