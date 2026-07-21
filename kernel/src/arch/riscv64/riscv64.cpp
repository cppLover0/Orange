
#include <cstdint>
#include <generic/arch.hpp>
#include <arch/riscv64/features.hpp>

namespace arch {
    void disable_interrupts() {
        asm volatile("csrc sstatus, %0" : : "r"(1 << 1)); 
    }

    void enable_interrupts() {
        asm volatile("csrs sstatus, %0" : : "r"(1 << 1));
    }

    void wait_for_interrupt() {
        asm volatile("wfi"); 
    }
    
    void memory_barrier() {
        asm volatile("" ::: "memory"); 
    }

    bool test_interrupts() {
        uint64_t status;
        __asm__ __volatile__ (
            "csrr %0, mstatus" 
            : "=r" (status)
        );

        return (status & (1 << 3)) != 0;
    }

    void hcf() {
        disable_interrupts();
        while(true) {
            wait_for_interrupt();
        }
    }

    void pause() {
#ifdef __riscv_zihintpause
        asm volatile("pause");
#else
        asm volatile("nop");
#endif
    }

    void tlb_flush(std::uintptr_t hint, std::uintptr_t len) {
        if (len / PAGE_SIZE > 256 || len == 0) {
            asm volatile("sfence.vma");
        } else {
            for (std::uintptr_t i = 0; i < len; i += PAGE_SIZE) {
                asm volatile("sfence.vma %0" : : "r"(hint + i) : "memory");
            }
        }
    }

    const char* name() {
        return "riscv64";
    }

    int level_paging() {
        return riscv64::get_paging_level();
    }

    void init(int stage) {
        (void)stage;
    }

}