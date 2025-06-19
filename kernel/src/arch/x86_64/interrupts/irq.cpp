
#include <stdint.h>
#include <arch/x86_64/interrupts/irq.hpp>
#include <arch/x86_64/interrupts/ioapic.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <generic/memory/paging.hpp>
#include <other/assembly.hpp>
#include <other/log.hpp>

irq_t irq_table[255];
uint16_t irq_ptr = 0;

extern void* irqTable[];

extern "C" void irqHandler(int_frame_t* ctx) {
    
    __cli();
    LogUnlock();

    Paging::EnableKernel();
    
    irq_table[ctx->vec].func(irq_table[ctx->vec].arg);
    
    if(ctx->cs == 0x20) {
        ctx->cs |= 3;
        ctx->ss |= 3;
    }
    
    if(ctx->ss == 0x18) {
        ctx->cs |= 3;
        ctx->ss |= 3;
    }
    
    Lapic::EOI();

}

uint8_t IRQ::Create(uint16_t irq,uint8_t type,void (*func)(void* arg),void* arg,uint64_t flags) {
    
    uint8_t entry = IDT::AllocEntry();
    idt_entry_t* idt_entry = IDT::SetEntry(entry,irqTable[irq_ptr],0x8E);
    if(type == IRQ_TYPE_LEGACY)  // configure ioapic
        IOAPIC::SetEntry(entry,irq,flags,Lapic::ID());
    

    idt_entry->ist = 2;

    irq_table[irq_ptr].arg = arg;
    irq_table[irq_ptr++].func = func;

    return entry;

}