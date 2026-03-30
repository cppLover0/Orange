#include <klibc/stdio.hpp>
#include <arch/x86_64/panic.hpp>
#include <arch/x86_64/cpu/idt.hpp>
#include <generic/arch.hpp>

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

    //stackframe_t* rbp = (stackframe_t*)ctx->rbp;

    klibc::printf("[0] - 0x%016llX (current rip)\n\r",ctx->rip);
    // for (int i = 1; i < 5 && rbp; ++i) {
    //     std::uint64_t ret_addr = rbp->rip;
    //     klibc::printf("[%d] - 0x%016llX\n\r", i, ret_addr);
    //     rbp = (stackframe_t*)rbp->rbp;
    // }

}

std::uint8_t gaster[] = {
#embed "src/gaster.txt"
};

void x86_64::panic::print_ascii_art() {
    klibc::printf("%s\n",gaster);
}

extern "C" void CPUKernelPanic(x86_64::idt::int_frame_t* frame) {
    x86_64::panic::print_ascii_art();
    print_regs(frame);
    arch::hcf();
}