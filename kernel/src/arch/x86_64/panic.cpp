#include <klibc/stdio.hpp>
#include <arch/x86_64/panic.hpp>
#include <arch/x86_64/cpu/idt.hpp>
#include <generic/arch.hpp>
#include <generic/vmm.hpp>
#include <arch/x86_64/cpu_local.hpp>
#include <generic/scheduling.hpp>
#include <generic/arch.hpp>

void print_regs(x86_64::idt::int_frame_t* ctx) {

    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");
    klibc::printf("Kernel panic\n\nReason: %s\n\r","cpu exception");
    klibc::printf("RAX: 0x%016llX RBX: 0x%016llX RDX: 0x%016llX RCX: 0x%016llX RDI: 0x%016llX\n\r"
         "RSI: 0x%016llX  R8: 0x%016llX  R9: 0x%016llX R10: 0x%016llX R11: 0x%016llX\n\r"
         "R12: 0x%016llX R13: 0x%016llX R14: 0x%016llX R15: 0x%016llX RBP: 0x%016llX\n\r"
         "RSP: 0x%016llX CR2: 0x%016llX CR3: 0x%016llX\n\r"
         "Vec: %d Err: %d cs: %p ss: %p fs_base 0x%p\n\r",
         ctx->rax, ctx->rbx, ctx->rdx, ctx->rcx, ctx->rdi,
         ctx->rsi, ctx->r8, ctx->r9, ctx->r10, ctx->r11,
         ctx->r12, ctx->r13, ctx->r14, ctx->r15, ctx->rbp,
         ctx->rsp, cr2, ctx->cr3, ctx->vec, ctx->err_code, ctx->cs,ctx->ss, assembly::rdmsr(0xC0000100));
    klibc::printf("\n\r    Stacktrace\n\r\n\r");
  
    stackframe_t* rbp = (stackframe_t*)ctx->rbp;

    klibc::printf("[0] - 0x%016llX (current rip)\n\r",ctx->rip);
    for (int i = 1; i < 30 && rbp; ++i) {
        if((std::uint64_t)rbp < etc::hhdm())
            break;
        std::uint64_t ret_addr = rbp->rip;
        klibc::printf("[%d] - 0x%016llX\n\r", i, ret_addr);
        rbp = (stackframe_t*)rbp->rbp;
    }

}

std::uint8_t gaster[] = {
#embed "src/gaster.txt"
};

void x86_64::panic::print_ascii_art() {
    klibc::printf("%s\n",gaster);
} 

#include <generic/lock/spinlock.hpp>
#include <arch/x86_64/cpu/lapic.hpp>

extern "C" void CPUKernelPanic(x86_64::idt::int_frame_t* frame) {

    arch::enable_paging(gobject::kernel_root);

    if(frame->cs != 0x08)
        asm volatile("swapgs");

    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");

    x86_64::cpu_data();

    thread* current_thread = CPU_LOCAL_READ(current_thread);

    if(frame->vec == 14 && cr2 > PAGE_SIZE && current_thread != nullptr) {
        bool state = current_thread->vmem->lock.lock();
        vmm_obj* obj = current_thread->vmem->nlgetlen(cr2);
        if(obj != nullptr) {
            bool status = current_thread->vmem->inv_lazy_alloc(ALIGNPAGEDOWN(cr2), PAGE_SIZE);
            if(status == true) {
                arch::tlb_flush(ALIGNPAGEDOWN(cr2), PAGE_SIZE);
                current_thread->vmem->lock.unlock(state);

                if(frame->cs & 3)
                    frame->ss |= 3;
                                        
                if(frame->ss & 3)
                    frame->cs |= 3;

                if(frame->cs == 0x20)
                    frame->cs |= 3;
                                        
                if(frame->ss == 0x18)
                    frame->ss |= 3;

                if(frame->cs != 0x08)
                    asm volatile("swapgs");

                process::switch_ctx(frame);
                __builtin_unreachable();
            }
        }
        current_thread->vmem->lock.unlock(state);
    }

    x86_64::lapic::off();
    locks::is_disabled = true;
    x86_64::panic::print_ascii_art();
    print_regs(frame);
    extern int k_breakpoint;
    klibc::printf("k_breakpoint is %d, proc is %d, cpu is %d, target_cpu %d\r\n",k_breakpoint, current_thread == nullptr ? 0 : current_thread->id, CPU_LOCAL_READ(cpu),current_thread == nullptr ? 0 : current_thread->cpu.load());
    
    if(current_thread) {
        if(current_thread->vmem) {
            klibc::printf("memory map\n");
            vmm_obj* current = current_thread->vmem->start;
            while(current) {

                if(current == current_thread->vmem->end)
                    break;

                klibc::printf("memory 0x%p-0x%p len %lli, rip - base is 0x%p\n",current->base, current->base + current->len, current->len, frame->rip - current->base);

                current = current->next;
            }
        }
    }
    
    arch::hcf();
}