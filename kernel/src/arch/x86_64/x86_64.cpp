
#include <cstdint>
#include <generic/arch.hpp>
#include <generic/bootloader/bootloader.hpp>
#include <arch/x86_64/drivers/hpet.hpp>
#include <arch/x86_64/cpu_local.hpp>
#include <arch/x86_64/drivers/tsc.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/cpu/idt.hpp>
#include <klibc/stdio.hpp>

typedef struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} __attribute__((packed)) stackframe_t;

void print_regs(x86_64::idt::int_frame_t* ctx) {
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");
    klibc::printf("Kernel panic\n\nReason: %s\n\r","cpu exception");
    klibc::printf("RAX: 0x%016llX RBX: 0x%016llX RDX: 0x%016llX RCX: 0x%016llX RDI: 0x%016llX\n\r"
         "RSI: 0x%016llX  R8: 0x%016llX  R9: 0x%016llX R10: 0x%016llX R11: 0x%016llX\n\r"
         "R12: 0x%016llX R13: 0x%016llX R14: 0x%016llX R15: 0x%016llX RBP: 0x%016llX\n\r"
         "RSP: 0x%016llX CR2: 0x%016llX CR3: 0x%016llX\n\r"
         "Vec: %d Err: %d cs: %p ss: %p\n\r",
         ctx->rax, ctx->rbx, ctx->rdx, ctx->rcx, ctx->rdi,
         ctx->rsi, ctx->r8, ctx->r9, ctx->r10, ctx->r11,
         ctx->r12, ctx->r13, ctx->r14, ctx->r15, ctx->rbp,
         ctx->rsp, cr2, ctx->cr3, ctx->vec, ctx->err_code, ctx->cs,ctx->ss);
    klibc::printf("\n\r    Stacktrace\n\r\n\r");

    stackframe_t* rbp = (stackframe_t*)ctx->rbp;

    klibc::printf("[0] - 0x%016llX (current rip)\n\r",ctx->rip);
    for (int i = 1; i < 5 && rbp; ++i) {
        std::uint64_t ret_addr = rbp->rip;
        klibc::printf("[%d] - 0x%016llX\n\r", i, ret_addr);
        rbp = (stackframe_t*)rbp->rbp;
    }

}

std::uint8_t gaster[] = {
#embed "src/gaster.txt"
};

void print_ascii_art() {
    klibc::printf("%s\n",gaster);
}

extern "C" void CPUKernelPanic(x86_64::idt::int_frame_t* frame) {
    print_ascii_art();
    print_regs(frame);
    arch::hcf();
}

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
            return;
        case ARCH_INIT_COMMON:
            x86_64::gdt::init();
            x86_64::idt::init();
            return;
        }
    }

    [[gnu::weak]] void panic(char* msg) {
        print_ascii_art();
        klibc::printf("Panic with message \"%s\"",msg);
    }

}