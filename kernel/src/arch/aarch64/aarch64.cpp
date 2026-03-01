
#include <cstdint>
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
            asm volatile("tlbi alle1\n" "dsb ish\n" "isb\n" : : : "memory");
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

    [[gnu::weak]] void init(int stage) {
        (void)stage;
    }

}