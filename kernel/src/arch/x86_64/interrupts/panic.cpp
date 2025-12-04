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

int is_panic = 0;

void panic(int_frame_t* ctx, const char* msg) {

    memory::paging::enablekernel();
    if(is_panic)
        asm volatile("hlt");

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;
    if(proc && ctx) {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");

        vmm_obj_t* current1 = (vmm_obj_t*)proc->vmm_start;
        current1->lock.unlock();

        vmm_obj_t* obj = memory::vmm::getlen(proc,ctx->rip);


        if(ctx->vec == 14) {
            vmm_obj_t* obj0 = memory::vmm::getlen(proc,cr2);
            if(obj0) {
                if(obj0->is_lazy_alloc) {
                    std::uint64_t phys = memory::pmm::_physical::alloc(obj0->src_len);
                    obj0->phys = phys;

                    memory::paging::maprangeid(proc->original_cr3,obj0->phys,obj0->base,obj0->len,obj0->flags,*proc->vmm_id);

                    if(ctx->cs & 3)
                        ctx->ss |= 3;
                        
                    if(ctx->ss & 3)
                        ctx->cs |= 3;

                    if(ctx->cs == 0x20)
                        ctx->cs |= 3;
                        
                    if(ctx->ss == 0x18)
                        ctx->ss |= 3;

                    obj0->is_lazy_alloc = 0;

                    schedulingEnd(ctx);
                } 
            }
        }

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

    int_frame_t test_ctx = {0};
    ctx = &test_ctx;

    is_panic = 1;

    extern locks::spinlock log_lock;
    log_lock.unlock();

    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");

    Log::Display(LEVEL_MESSAGE_FAIL,R"EOF(              
                                                
    )                                                                  
 ( /(                                       (             (            
 )\()) (      )       (  (    (    (        )\ )  (    )  )\ )         
((_)\  )(  ( /(  (    )\))(  ))\   )\ (    (()/( ))\( /( (()/(      __ 
  ((_)(()\ )(_)) )\ )((_))\ /((_) ((_))\    ((_))((_)(_)) ((_))  _ / / 
 / _ \ ((_|(_)_ _(_/( (()(_|_))    (_|(_)   _| (_))((_)_  _| |  (_) |  
| (_) | '_/ _` | ' \)) _` |/ -_)   | (_-< / _` / -_) _` / _` |   _| |  
 \___/|_| \__,_|_||_|\__, |\___|   |_/__/ \__,_\___\__,_\__,_|  (_)\_\ 
                     |___/                                                                                   

    )EOF");

    Log::Raw("Kernel panic\n\nReason: %s\n",msg);
    Log::Raw("RAX: 0x%016llX RBX: 0x%016llX RDX: 0x%016llX RCX: 0x%016llX RDI: 0x%016llX\n"
         "RSI: 0x%016llX  R8: 0x%016llX  R9: 0x%016llX R10: 0x%016llX R11: 0x%016llX\n"
         "R12: 0x%016llX R13: 0x%016llX R14: 0x%016llX R15: 0x%016llX RBP: 0x%016llX\n"
         "RSP: 0x%016llX CR2: 0x%016llX CR3: 0x%016llX\n"
         "CPU: %d Vec: %d Err: %d\n",
         ctx->rax, ctx->rbx, ctx->rdx, ctx->rcx, ctx->rdi,
         ctx->rsi, ctx->r8, ctx->r9, ctx->r10, ctx->r11,
         ctx->r12, ctx->r13, ctx->r14, ctx->r15, ctx->rbp,
         ctx->rsp, cr2, ctx->cr3,
         arch::x86_64::cpu::data()->smp.cpu_id, ctx->vec, ctx->err_code);
    Log::Raw("\n    Stacktrace\n\n");

    stackframe_t* rbp = (stackframe_t*)ctx->rbp;

    Log::Raw("[0] - 0x%016llX (current rip)\n",ctx->rip);
    for (int i = 1; i < 5 && rbp; ++i) {
        std::uint64_t ret_addr = rbp->rip;
        Log::Raw("[%d] - 0x%016llX\n", i, ret_addr);
        rbp = (stackframe_t*)rbp->rbp;
    }


    asm volatile("hlt");
}

extern "C" void CPUKernelPanic(int_frame_t* ctx) {
    panic(ctx,"\"cpu exception\"");
}