#include <cstdint>
#include <arch/x86_64/interrupts/idt.hpp>
#include <etc/logging.hpp>

void panic(int_frame_t* ctx, const char* msg) {
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");
    Log::Display(LEVEL_MESSAGE_FAIL,"Got exception \"%s\" with rip 0x%p, vec %d, cr2 0x%p and error code 0x%p !\n",msg,ctx->rip,ctx->vec,cr2,ctx->err_code);
    asm volatile("hlt");
}

extern "C" void CPUKernelPanic(int_frame_t* ctx) {
    panic(ctx,"CPU Exception");
}