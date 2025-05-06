
#include <arch/x86_64/interrupts/idt.hpp>
#include <stdint.h>
#include <drivers/serial/serial.hpp>
#include <other/assembly.hpp>
#include <other/log.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/memory/paging.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <generic/memory/vmm.hpp>
   

typedef struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} __attribute__((packed)) stackframe_t;

extern "C" void CPanic(const char* msg,int_frame_t* frame1) {
    __cli();
    Paging::EnableKernel();
    LogUnlock();

    int_frame_t* frame = frame1;

    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");

    NLog("\n\n\nKernel panic\n");
   
    NLog("\nReason: %s\nCPU: %d\nVector: %d/0x%p (error code: %d/0x%p)\nProc: 0x%p, ID: %d, is_user: %d start_rsp: 0x%p stack_end: 0x%p proc_userspace_stack: 0x%p\n",msg,Lapic::ID(),frame->vec,frame->vec,frame->err_code,frame->err_code,CpuData::Access()->current,CpuData::Access()->current ? CpuData::Access()->current->id : 0,CpuData::Access()->current ? CpuData::Access()->current->user : 0,CpuData::Access()->current ? CpuData::Access()->current->stack_start : 0,CpuData::Access()->current ? (uint64_t)CpuData::Access()->current->stack : 0,CpuData::Access()->current ? (uint64_t)CpuData::Access()->current->user_stack_start : 0);
    NLog("Proc FS: 0x%p\n",CpuData::Access()->current ? CpuData::Access()->current->fs_base : 0);
    NLog("Registers dump\n");
    NLog("RIP: 0x%p, SS: 0x%p, CS: 0x%p, RFLAGS: 0x%p, RSP: 0x%p\nRAX: 0x%p, RBX: 0x%p, RCX: 0x%p, RDX: 0x%p, RSI: 0x%p\nRDI: 0x%p, RBP: 0x%p, R8: 0x%p, R9: 0x%p, R10: 0x%p, CR2: 0x%p\nR11: 0x%p, R12: 0x%p, R13: 0x%p, R14: 0x%p, R15: 0x%p CR3: 0x%p\n",
    frame->rip,frame->ss,frame->cs,frame->rflags,frame->rsp,frame->rax,frame->rbx,frame->rcx,frame->rdx,frame->rsi,frame->rdi,frame->rbp,frame->r8,frame->r9,frame->r10,cr2,frame->r11,frame->r12,frame->r13,frame->r14,frame->r15,frame->cr3);
    
    NLog("Kernel cr3: 0x%p\n",Paging::KernelGet());

    cpudata_t* data = CpuData::Access();
    NLog("CPU Data: 0x%p, UserStack: 0x%p, KernelStack: 0x%p\n",CpuData::Access(),CpuData::Access()->user_stack,CpuData::Access()->kernel_stack);
    asm volatile("swapgs");
    NLog("CPU Data: 0x%p, UserStack: 0x%p, KernelStack: 0x%p\n",CpuData::Access(),CpuData::Access()->user_stack,CpuData::Access()->kernel_stack);
    asm volatile("swapgs");

    process_t* proc = CpuData::Access()->current;
    
    if(!proc) {// exception occured in kernel, we can backtrace 
        
        stackframe_t* rbp = (stackframe_t*)frame1->rbp;

        NLog("\nBack Trace\n");
        for(unsigned int frame = 0; rbp && frame < 10; ++frame)
        {
            NLog("Address #%d: 0x%p\n",frame,rbp->rip);
            rbp = rbp->rbp;
        }
    
    }


    if(proc){

        VMM::Dump(proc);

        if(proc->parent_process) {
            process_t* parent = Process::ByID(proc->parent_process);
            if(parent) {
                frame = &parent->ctx;

                NLog("\nParent: id: %d, is_user: %d, start_rsp: 0x%p, stack_end: 0x%p\n",parent->id,parent->user,parent->stack_start,parent->stack);

                NLog("\nParent Registers dump\n");
                NLog("RIP: 0x%p, SS: 0x%p, CS: 0x%p, RFLAGS: 0x%p, RSP: 0x%p\nRAX: 0x%p, RBX: 0x%p, RCX: 0x%p, RDX: 0x%p, RSI: 0x%p\nRDI: 0x%p, RBP: 0x%p, R8: 0x%p, R9: 0x%p, R10: 0x%p, CR2: 0x%p\nR11: 0x%p, R12: 0x%p, R13: 0x%p, R14: 0x%p, R15: 0x%p CR3: 0x%p\n",
                frame->rip,frame->ss,frame->cs,frame->rflags,frame->rsp,frame->rax,frame->rbx,frame->rcx,frame->rdx,frame->rsi,frame->rdi,frame->rbp,frame->r8,frame->r9,frame->r10,cr2,frame->r11,frame->r12,frame->r13,frame->r14,frame->r15,frame->cr3);
                
                NLog("Stack difference: 0x%p\n",((uint64_t)parent->stack - parent->ctx.rsp));

                VMM::Dump(parent);

            }
        }
    }

    __hlt();
}

extern "C" void CPUKernelPanic(int_frame_t* frame) {
    CPanic("CPU Exception",frame);
    __cli();
    __hlt();
}
