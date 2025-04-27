
#include <arch/x86_64/interrupts/idt.hpp>
#include <stdint.h>
#include <drivers/serial/serial.hpp>
#include <other/assembly.hpp>
#include <other/log.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/memory/paging.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
        
extern "C" void CPanic(const char* msg,int_frame_t* frame) {
    __cli();
    Paging::EnableKernel();
    LogUnlock();

    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2) : : "memory");

    NLog("\n\n\nKernel panic\n");
   
    NLog("\nReason: %s\nCPU: %d\nVector: %d/0x%p (error code: %d/0x%p)\nProc: 0x%p, ID: %d, is_user: %d\n",msg,Lapic::ID(),frame->vec,frame->vec,frame->err_code,frame->err_code,CpuData::Access()->current,CpuData::Access()->current ? CpuData::Access()->current->id : 0,CpuData::Access()->current ? CpuData::Access()->current->user : 0);
    NLog("Registers dump\n");
    NLog("RIP: 0x%p, SS: 0x%p, CS: 0x%p, RFLAGS: 0x%p, RSP: 0x%p\nRAX: 0x%p, RBX: 0x%p, RCX: 0x%p, RDX: 0x%p, RSI: 0x%p\nRDI: 0x%p, RBP: 0x%p, R8: 0x%p, R9: 0x%p, R10: 0x%p, CR2: 0x%p\nR11: 0x%p, R12: 0x%p, R13: 0x%p, R14: 0x%p, R15: 0x%p CR3: 0x%p\n",
    frame->rip,frame->ss,frame->cs,frame->rflags,frame->rsp,frame->rax,frame->rbx,frame->rcx,frame->rdx,frame->rsi,frame->rdi,frame->rbp,frame->r8,frame->r9,frame->r10,cr2,frame->r11,frame->r12,frame->r13,frame->r14,frame->r15,frame->cr3);
    
    cpudata_t* data = CpuData::Access();
    NLog("CPU Data: 0x%p, UserStack: 0x%p, KernelStack: 0x%p\n",CpuData::Access(),CpuData::Access()->user_stack,CpuData::Access()->kernel_stack);

    __hlt();
}

extern "C" void CPUKernelPanic(int_frame_t* frame) {
    CPanic("CPU Exception",frame);
    __cli();
    __hlt();
}
