
#include <cstdint>
#include <generic/arch.hpp>
#include <generic/bootloader/bootloader.hpp>
#include <arch/x86_64/drivers/hpet.hpp>
#include <arch/x86_64/cpu_local.hpp>
#include <arch/x86_64/drivers/tsc.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/cpu/idt.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/panic.hpp>
#include <arch/x86_64/irq.hpp>
#include <klibc/stdio.hpp>
#include <arch/x86_64/drivers/ioapic.hpp>

namespace arch {
    [[gnu::weak]] void disable_interrupts() {
        asm volatile("cli");
    }

    [[gnu::weak]] void enable_interrupts() {
        asm volatile("sti");
    }

    [[gnu::weak]] void wait_for_interrupt() {
        asm volatile("hlt");
    }

    [[gnu::weak]] void hcf() {
        disable_interrupts();
        while(true) {
            wait_for_interrupt();
        }
    }

    [[gnu::weak]] void pause() {
        asm volatile("pause");
    }

    [[gnu::weak]] void tlb_flush(std::uintptr_t hint, std::uintptr_t len) {
        if (len / PAGE_SIZE > 256 || len == 0) {
            std::uint64_t cr3 = 0;
            asm volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
            asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
        } else {
            for (std::uintptr_t i = 0; i < len; i += PAGE_SIZE) {
                asm volatile("invlpg (%0)" : : "b"(hint + i) : "memory");
            }
        }
    }

    [[gnu::weak]] const char* name() {
        return "x86_64";
    }

    [[gnu::weak]] int level_paging() {
        return bootloader::bootloader->is_5_level_paging() ? 5 : 4;
    }

    [[gnu::weak]] void init(int stage) {
        switch(stage) {
        case ARCH_INIT_EARLY:
            x86_64::init_cpu_data();
            drivers::hpet::init();
            drivers::tsc::init();
            x86_64::gdt::init();
            x86_64::idt::init();
            x86_64::lapic::init(1500);
            drivers::ioapic::init();
            return;
        case ARCH_INIT_COMMON:
            return;
        }
    }

    [[gnu::weak]] void panic(char* msg) {
        x86_64::panic::print_ascii_art();
        klibc::printf("Panic with message \"%s\"\r\n",msg);
    }

    [[gnu::weak]] int register_handler(int irq, int type, std::uint64_t flags, void (*func)(void* arg), void* arg) {
        return x86_64::irq::create(irq,type,func,arg,flags);
    }

}