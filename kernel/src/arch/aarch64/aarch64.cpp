
#include <cstdint>
#include <arch/aarch64/cpu/el.hpp>
#include <arch/aarch64/cpu/timer.hpp>
#include <generic/arch.hpp>

namespace arch {
    [[gnu::weak]] void disable_interrupts() {
        asm volatile("msr daifset, #2"); 
    }

    [[gnu::weak]] void enable_interrupts() {
        asm volatile("msr daifclr, #2"); 
    }

    [[gnu::weak]] void wait_for_interrupt() {
        asm volatile("wfi"); 
    }

    [[gnu::weak]] void hcf() {
        disable_interrupts();
        while(true) {
            wait_for_interrupt();
        }
    }

    [[gnu::weak]] void tlb_flush(std::uintptr_t hint, std::uintptr_t len) {
        if (len / PAGE_SIZE > 256 || len == 0) {
            __asm__ volatile("dsb ishst" : : : "memory");
            __asm__ volatile("tlbi vmalle1is" : : : "memory");
            __asm__ volatile("dsb ish" : : : "memory");
            __asm__ volatile("isb" : : : "memory");
        } else {
            for (std::uintptr_t i = 0; i < len; i += PAGE_SIZE) {
                std::uintptr_t addr = hint + i;
                asm volatile("tlbi vae1, %0\n" : : "r"(addr) : "memory");
            }

            asm volatile("dsb ish\n" "isb\n" : : : "memory");
        }
    }

    [[gnu::weak]] void pause() {
        asm volatile("isb");
    }

    [[gnu::weak]] const char* name() {
        return "aarch64";
    }

    [[gnu::weak]] int level_paging() {
        return 4;
    }

    [[gnu::weak]] void memory_barrier() {
        asm volatile("" ::: "memory"); 
    }

    [[gnu::weak]] bool test_interrupts() {
        uint64_t daif;
        __asm__ __volatile__ (
            "mrs %0, daif"
            : "=r" (daif)
        );
        return (daif & (1 << 7)) == 0;
    }

    [[gnu::weak]] void init(int stage) {
        switch(stage) {
        case ARCH_INIT_EARLY:
            aarch64::el::init();
            aarch64::timer::init();
            return;
        case ARCH_INIT_COMMON:
            return;
        } 
    }

}