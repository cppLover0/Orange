#include <cstdint>
#include <arch/x86_64/interrupts/idt.hpp>
#include <etc/logging.hpp>

#include <arch/x86_64/cpu/data.hpp>

#include <generic/locks/spinlock.hpp>

#include <etc/bootloaderinfo.hpp>
#include <generic/mm/vmm.hpp>

#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/paging.hpp>

typedef struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} __attribute__((packed)) stackframe_t;



void panic(int_frame_t* ctx, const char* msg) {

    memory::paging::enablekernel();

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;
    if(proc) {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");
        vmm_obj_t* obj = memory::vmm::getlen(proc,ctx->rip);
        Log::SerialDisplay(LEVEL_MESSAGE_FAIL,"process %d fired cpu exception with vec %d, rip 0x%p (offset 0x%p), cr2 0x%p, error code 0x%p, lastsys %d, rdx 0x%p\n",proc->id,ctx->vec,ctx->rip,ctx->rip - 0x41400000,cr2,ctx->err_code,proc->sys,ctx->rdx);
        
        vmm_obj_t* current = (vmm_obj_t*)proc->vmm_start;

        while(1) {

            Log::Raw("Memory 0x%p-0x%p (with offset rip 0x%p)\n",current->base,current->base + current->src_len, ctx->rip - current->base);

            if (current->base == (std::uint64_t)Other::toVirt(0) - 4096)
                    break;

            current = current->next;

        }

        arch::x86_64::scheduling::kill(proc);
       schedulingSchedule(0);
    }

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