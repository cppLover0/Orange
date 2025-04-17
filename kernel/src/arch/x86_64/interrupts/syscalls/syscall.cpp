
#include <arch/x86_64/interrupts/syscalls/syscall.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <other/log.hpp>
#include <stdint.h>
#include <other/assembly.hpp>
#include <drivers/hpet/hpet.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/pmm.hpp>
#include <generic/memory/heap.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <config.hpp>
#include <other/string.hpp>

extern "C" void syscall_handler();

// syscalls

int syscall_exit(int_frame_t* ctx) {

    process_t* proc = CpuData::Access()->current;
    proc->return_status = ctx->rdi;
    proc->status = PROCESS_STATUS_KILLED;
    
    if(proc->stack_start) {
        PMM::VirtualBigFree((void*)proc->stack_start,PROCESS_STACK_SIZE);
    }

    ctx->cr3 = 0; // just schedule
    return 0;
}

int syscall_debug_print(int_frame_t* ctx) {

    uint64_t size = ctx->rsi;

    char* ptr = (char*)KHeap::Malloc(size);
    char* src = ctx->rdi;

    process_t* proc = CpuData::Access()->current;

    if(ctx->cr3) { 



        Paging::EnablePaging(ctx->cr3);
        String::memcpy(ptr,src,size);
        Paging::EnableKernel();

        Log("[Process %d]: %s\n",proc->id,ptr);

        ctx->rax = 0;


    } else {
        ctx->rax = -1;
    }

    KHeap::Free((void*)ptr);
    return;

}

syscall_t syscall_table[] = {
    {1,syscall_exit},
    {2,syscall_debug_print}
};

syscall_t* syscall_find_table(int num) {
    for(int i = 0;i < sizeof(syscall_table) / sizeof(syscall_t);i++) {
        if(syscall_table[i].num == num)
            return &syscall_table[i];
    }
    return 0; // :(
}

extern "C" void c_syscall_handler(int_frame_t* ctx) {
    Paging::EnableKernel();
    syscall_t* sys = syscall_find_table(ctx->rax);
    
    if(sys == 0) {
        ctx->rax = -1;
        return;
    }

    ctx->rax = sys->func(ctx);

    if(ctx->cr3 = 0) { // just schedule, skip this
        __sti();
        __hlt();
    }

    return;
}

// end of syscalls

void Syscall::Init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,(1 << 9)); // disable only IF
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}