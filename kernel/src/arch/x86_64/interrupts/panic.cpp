
#include <arch/x86_64/interrupts/idt.hpp>
#include <stdint.h>
#include <drivers/serial/serial.hpp>
#include <other/assembly.hpp>
#include <other/log.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/memory/paging.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <generic/memory/vmm.hpp>
#include <generic/memory/pmm.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>

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

    if(CpuData::Access()->current) {
        process_t* proc = CpuData::Access()->current;
        fd_t* fd = (fd_t*)proc->start_fd;
        while(fd) {
            fd = fd->next;
            if(fd) {

                if(fd->type == FD_FILE)
                    VFS::Count(fd->path_point,0,-1);

                if(fd->is_pipe_pointer && fd->type == FD_PIPE && fd->pipe_side == PIPE_SIDE_WRITE) {
                    if(fd->p_pipe->connected_pipes <= 0) {
                        PMM::VirtualFree(fd->p_pipe->buffer);
                    }
                    fd->p_pipe->connected_pipes--;
                    if(fd->p_pipe->connected_pipes == 0) {
                        fd->p_pipe->is_received = 0;
                    }
                } else if(!fd->is_pipe_pointer) {
                    PMM::VirtualFree(fd->pipe.buffer);
                }
                PMM::VirtualFree(fd);
            }
            
        }

        Process::Kill(proc,9);
        
        ERROR("Got fault in process %d !\n",proc->id);
        schedulingSchedule(frame1);

    }

    NLog("\n\n\nKernel panic\n");
   
    NLog("\nReason: %s\nCPU: %d\nVector: %d/0x%p (error code: %d/0x%p)\nProc: 0x%p, ID: %d, is_user: %d start_rsp: 0x%p stack_end: 0x%p proc_userspace_stack: 0x%p\n",msg,Lapic::ID(),frame->vec,frame->vec,frame->err_code,frame->err_code,CpuData::Access()->current,CpuData::Access()->current ? CpuData::Access()->current->id : 0,CpuData::Access()->current ? CpuData::Access()->current->user : 0,CpuData::Access()->current ? CpuData::Access()->current->stack_start : 0,CpuData::Access()->current ? (uint64_t)CpuData::Access()->current->stack : 0,CpuData::Access()->current ? (uint64_t)CpuData::Access()->current->user_stack_start : 0);
    NLog("Proc FS: 0x%p, Last syscall: %d, originalcr3: 0x%p\n",CpuData::Access()->current ? CpuData::Access()->current->fs_base : 0,CpuData::Access()->last_syscall,CpuData::Access()->current ? CpuData::Access()->current->nah_cr3 : 0);
    NLog("Registers dump\n");
    NLog("RIP: 0x%p, SS: 0x%p, CS: 0x%p, RFLAGS: 0x%p, RSP: 0x%p\nRAX: 0x%p, RBX: 0x%p, RCX: 0x%p, RDX: 0x%p, RSI: 0x%p\nRDI: 0x%p, RBP: 0x%p, R8: 0x%p, R9: 0x%p, R10: 0x%p, CR2: 0x%p\nR11: 0x%p, R12: 0x%p, R13: 0x%p, R14: 0x%p, R15: 0x%p CR3: 0x%p\n",
    frame->rip,frame->ss,frame->cs,frame->rflags,frame->rsp,frame->rax,frame->rbx,frame->rcx,frame->rdx,frame->rsi,frame->rdi,frame->rbp,frame->r8,frame->r9,frame->r10,cr2,frame->r11,frame->r12,frame->r13,frame->r14,frame->r15,frame->cr3);
    
    NLog("Kernel cr3: 0x%p\n",Paging::KernelGet());

    cpudata_t* data = CpuData::Access();
    NLog("CPU Data: 0x%p, UserStack: 0x%p, KernelStack: 0x%p\n",CpuData::Access(),CpuData::Access()->user_stack,CpuData::Access()->kernel_stack);
    asm volatile("swapgs");
    NLog("CPU Data: 0x%p, UserStack: 0x%p, KernelStack: 0x%p\n",CpuData::Access(),CpuData::Access()->user_stack,CpuData::Access()->kernel_stack);
    asm volatile("swapgs");

    NLog("4k page allocation: 0x%p\n",PMM::Alloc());

    process_t* proc = CpuData::Access()->current;
    
    if(!proc) {// exception occured in kernel, we can backtrace 
        
        stackframe_t* rbp = (stackframe_t*)frame1->rbp;

        NLog("\nBack Trace\n");
        for(unsigned int frame = 0; rbp && frame < 10; ++frame)
        {
            NLog("Address #%d: 0x%p\n",frame,rbp->rip);
            rbp = rbp->rbp;
        }
    
    } else if(proc && !proc->user) {
        stackframe_t* rbp = (stackframe_t*)frame1->rbp;

        NLog("\nBack Trace\n");
        for(unsigned int frame = 0; rbp && frame < 10; ++frame)
        {
            NLog("Address #%d: 0x%p\n",frame,rbp->rip);
            rbp = rbp->rbp;
        }
    }


    if(0){

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

    while(1) {
        __cli();
        __hlt();
    }
}

extern "C" void CPUKernelPanic(int_frame_t* frame) {
    CPanic("CPU Exception",frame);
    __cli();
    __hlt();
}
