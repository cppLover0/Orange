
#include <cstdint>
#include <arch/x86_64/interrupts/irq.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/interrupts/pic.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <generic/mm/paging.hpp>
#include <drivers/ioapic.hpp>
#include <etc/libc.hpp>

#include <etc/logging.hpp>

irq_t irq_table[255];
std::uint16_t irq_ptr = 0;

std::uint8_t is_legacy_pic = 1;

extern "C" void* irqTable[];

extern "C" void irqHandler(int_frame_t* ctx) {

    memory::paging::enablekernel();

    irq_table[ctx->vec - 1].func(irq_table[ctx->vec - 1].arg);
    if(is_legacy_pic) {
        arch::x86_64::interrupts::pic::eoi(ctx->vec);
    } else {
        arch::x86_64::cpu::lapic::eoi();
    }

}

void arch::x86_64::interrupts::irq::reset() {
    irq_ptr = 0;
    memset(irq_table,0,sizeof(irq_table));
}

std::uint8_t arch::x86_64::interrupts::irq::create(std::uint16_t irq,std::uint8_t type,void (*func)(void* arg),void* arg,std::uint64_t flags) {
    uint8_t entry = 0;
    if(!is_legacy_pic) {
        entry = arch::x86_64::interrupts::idt::alloc();
        arch::x86_64::interrupts::idt::set_entry((std::uint64_t)irqTable[entry - 0x20],entry,0x8E,3);
        drivers::ioapic::set(entry,irq,0,arch::x86_64::cpu::lapic::id());
    } else {
        arch::x86_64::interrupts::idt::set_entry((std::uint64_t)irqTable[irq],irq + 0x20,0x8E,3);
        irq_table[irq].arg = arg;
        irq_table[irq].func = func;
        return irq + 0x20;
    }

    irq_table[irq_ptr].arg = arg;
    irq_table[irq_ptr++].func = func;
    return entry;
}