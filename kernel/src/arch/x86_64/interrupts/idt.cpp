
#include <stdint.h>
#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/memory/pmm.hpp>
#include <other/string.hpp>

idt_entry_t idt[256];
uint8_t vectors_idt[256];
idtr_t idtr;

idt_entry_t* IDT::SetEntry(uint8_t vec,void* base,uint8_t flags) {
    idt_entry_t* entry = &idt[vec];
    entry->base_low = (uint64_t)base & 0xFFFF;
    entry->cs = 0x08;
    entry->ist = 0;
    entry->attr = flags;
    entry->base_mid = ((uint64_t)base >> 16) & 0xFFFF;
    entry->base_high = ((uint64_t)base >> 32) & 0xFFFFFFFF;
    entry->reserved = 0;
    vectors_idt[vec] = 1;
    return entry;
}

uint8_t IDT::AllocEntry() {
    for(uint8_t vec = 32;vec < 256;vec++) {
        if(!vectors_idt[vec])
            return vec;
    }
    return 0;
} 

void IDT::FreeEntry(uint8_t vec) {
    vectors_idt[vec] = 0;
}

extern void* isrTable[];
extern void* ignoreStub;

void IDT::Load() {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

void IDT::Init() {
    
    String::memset(vectors_idt,0,sizeof(char) * 256);

    idtr.base = (uint64_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * 256 - 1;
    
    for(uint8_t vec = 0;vec <32;vec++){
        idt_entry_t* entry = SetEntry(vec,isrTable[vec],0x8E);
        entry->ist = 1;
    }
    for(uint8_t vec = 32;vec < 255;vec++) {
        idt_entry_t* entry = SetEntry(vec,ignoreStub,0x8E);
        vectors_idt[vec] = 0;
        entry->ist = 2;
    }
    __asm__ volatile ("lidt %0" : : "m"(idtr));

}