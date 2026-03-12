#include <arch/x86_64/cpu/idt.hpp>
#include <arch/x86_64/irq.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/drivers/ioapic.hpp>
#include <arch/x86_64/cpu/xapic.hpp>
#include <cstdint>

irq_t irq_table[255];
std::uint16_t irq_ptr = 0;

extern "C" void* irqTable[];

extern "C" void irqHandler(x86_64::idt::int_frame_t* ctx) {

    if(ctx->cs != 0x08)
        asm volatile("swapgs");

    irq_table[ctx->vec - 1].func(irq_table[ctx->vec - 1].arg);

    x86_64::apic::eoi();

    if(ctx->cs & 3)
        ctx->ss |= 3;
                        
    if(ctx->ss & 3)
        ctx->cs |= 3;

    if(ctx->cs == 0x20)
        ctx->cs |= 3;
                        
    if(ctx->ss == 0x18)
        ctx->ss |= 3;

    if(ctx->cs != 0x08)
        asm volatile("swapgs");

}

std::uint8_t x86_64::irq::create(std::uint16_t irq,std::uint8_t type,void (*func)(void* arg),void* arg,std::uint64_t flags) {
     uint8_t entry = 0;

    if(type == IRQ_TYPE_LEGACY) {
        entry = x86_64::idt::alloc();
        x86_64::idt::set_entry((std::uint64_t)irqTable[entry - 0x20],entry,0x8E,3);
        drivers::ioapic::set(entry,irq,flags,x86_64::lapic::id());
    } else {
        entry = x86_64::idt::alloc();
        x86_64::idt::set_entry((std::uint64_t)irqTable[entry - 0x20],entry,0x8E,3);
    }

    irq_table[irq_ptr].arg = arg;
    irq_table[irq_ptr].irq = irq;
    irq_table[irq_ptr++].func = func;
    return entry;
}