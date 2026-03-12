
#include <cstdint>
#include <arch/x86_64/cpu/idt.hpp>
#include <utils/bitmap.hpp>
#include <klibc/stdio.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <utils/assert.hpp>

x86_64::idt::idt_entry_t idt_entries[255];
utils::bitmap* idt_bitmap;
x86_64::idt::idtr_t idtr;

extern "C" void* isrTable[];

extern "C" void ignoreStubC() {
    assert(0,"ignore stub");
    x86_64::lapic::eoi();
    
}

void x86_64::idt::init() {

    static bool is_print = 0;

    if(!is_print) {
        klibc::memset(idt_entries,0,sizeof(idt_entries));
        idt_bitmap = new utils::bitmap(255);

        idtr.base = (std::uint64_t)idt_entries;
        idtr.limit = (std::uint16_t)sizeof(idt_entry_t) * 256 - 1;

        for(uint8_t vec = 0;vec <32;vec++){
            set_entry((std::uint64_t)isrTable[vec],vec,0x8E,1);
        }

        for(uint8_t vec = 32; vec < 255; vec++) {
            set_entry((std::uint64_t)ignoreStub,vec,0x8E,4);
            idt_bitmap->clear(vec);
        }

        idt_bitmap->set(32);
    }

    load();

    if(!is_print) {klibc::printf("IDT: IDTR is 0x%p\r\n",&idtr); is_print = 1;}
}

void x86_64::idt::load() {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

void x86_64::idt::set_entry(std::uint64_t base,std::uint8_t vec,std::uint8_t flags,std::uint8_t ist) {
    idt_entry_t* descriptor = &idt_entries[vec];

    descriptor->low = (std::uint64_t)base & 0xFFFF;
    descriptor->cs = 0x08;
    descriptor->ist = ist;
    descriptor->attr = flags;
    descriptor->mid = ((std::uint64_t)base >> 16) & 0xFFFF;
    descriptor->high = ((std::uint64_t)base >> 32) & 0xFFFFFFFF;
    descriptor->reserved0 = 0;

    idt_bitmap->set(vec);

}

std::uint8_t x86_64::idt::alloc() {
    for(int i = 0;i < 255;i++) {
        if(!idt_bitmap->test(i)) {
            idt_bitmap->set(i);
            return i;
        } 
    }
    return 0;
}