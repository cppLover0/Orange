
#include <cstdint>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <etc/libc.hpp>
#include <etc/list.hpp>

#include <etc/logging.hpp>

using namespace Lists;
using namespace arch::x86_64::interrupts;

idt_entry_t idt_entries[255];
Bitmap* idt_bitmap;
idtr_t idtr;

extern "C" void* isrTable[];

extern "C" void ignoreStubC() {
    arch::x86_64::cpu::lapic::eoi();
}

void arch::x86_64::interrupts::idt::init() {
    memset(idt_entries,0,sizeof(idt_entries));
    idt_bitmap = new Bitmap(255);

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
    load();
}

void arch::x86_64::interrupts::idt::load() {
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

void arch::x86_64::interrupts::idt::set_entry(std::uint64_t base,std::uint8_t vec,std::uint8_t flags,std::uint8_t ist) {
    idt_entry_t* descriptor = &idt_entries[vec];

    descriptor->low = (std::uint64_t)base & 0xFFFF;
    descriptor->cs = 0x08;
    descriptor->ist = ist;
    descriptor->attr = flags;
    descriptor->mid = ((std::uint64_t)base >> 16) & 0xFFFF;
    descriptor->high = ((std::uint64_t)base >> 32) & 0xFFFFFFFF;
    descriptor->reserved0 = 0;
    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"SETUP IST TO IDT %d %d\n",vec,ist);

    idt_bitmap->set(vec);

}

std::uint8_t arch::x86_64::interrupts::idt::alloc() {
    for(int i = 0;i < 255;i++)
        if(!idt_bitmap->test(i)) {
            idt_bitmap->set(i);
            return i;
        }
}