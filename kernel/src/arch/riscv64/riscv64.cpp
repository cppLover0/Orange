
#include <cstdint>
#include <generic/arch.hpp>
#include <arch/riscv64/features.hpp>

namespace arch {
    [[gnu::weak]] void disable_interrupts() {
        asm volatile("csrc sstatus, %0" : : "r"(1 << 1)); 
    }

    [[gnu::weak]] void enable_interrupts() {
        asm volatile("csrs sstatus, %0" : : "r"(1 << 1));
    }

    [[gnu::weak]] void wait_for_interrupt() {
        asm volatile("wfi"); 
    }
    
    [[gnu::weak]] void memory_barrier() {
        asm volatile("" ::: "memory"); 
    }

    [[gnu::weak]] bool test_interrupts() {
        uint64_t status;
        __asm__ __volatile__ (
            "csrr %0, mstatus" 
            : "=r" (status)
        );

        return (status & (1 << 3)) != 0;
    }

    [[gnu::weak]] void hcf() {
        disable_interrupts();
        while(true) {
            wait_for_interrupt();
        }
    }

    [[gnu::weak]] void pause() {
#ifdef __riscv_zihintpause
        asm volatile("pause");
#else
        asm volatile("nop");
#endif
    }

    [[gnu::weak]] void tlb_flush(std::uintptr_t hint, std::uintptr_t len) {
        if (len / PAGE_SIZE > 256 || len == 0) {
            asm volatile("sfence.vma");
        } else {
            for (std::uintptr_t i = 0; i < len; i += PAGE_SIZE) {
                asm volatile("sfence.vma %0" : : "r"(hint + i) : "memory");
            }
        }
    }

    [[gnu::weak]] const char* name() {
        return "riscv64";
    }

    [[gnu::weak]] int level_paging() {
        return riscv64::get_paging_level();
    }

    [[gnu::weak]] void init(int stage) {
        (void)stage;
    }

}