#include <cstdint>
#include <arch/x86_64/interrupts/idt.hpp>
#include <etc/logging.hpp>

#include <arch/x86_64/cpu/data.hpp>

#include <generic/locks/spinlock.hpp>

#include <etc/bootloaderinfo.hpp>

#include <generic/mm/paging.hpp>

typedef struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} __attribute__((packed)) stackframe_t;



void panic(int_frame_t* ctx, const char* msg) {

    memory::paging::enablekernel();
    extern locks::spinlock log_lock;
    log_lock.unlock();

    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");
    Log::Display(LEVEL_MESSAGE_FAIL,"Got exception \"%s\" with rip 0x%p, vec %d, cr2 0x%p and error code 0x%p \n",msg,ctx->rip,ctx->vec,cr2,ctx->err_code);
    asm volatile("hlt");
}

extern "C" void CPUKernelPanic(int_frame_t* ctx) {
    panic(ctx,"CPU Exception");
}