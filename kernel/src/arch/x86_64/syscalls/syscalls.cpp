
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <generic/locks/spinlock.hpp>

#include <arch/x86_64/scheduling.hpp>
#include <arch/x86_64/cpu/data.hpp>

#include <arch/x86_64/syscalls/signal.hpp>

#include <etc/errno.hpp>

extern "C" syscall_ret_t sys_hello(int a, int b ,int c) {
    DEBUG(1,"hello");
    return {0,0,0};
}

void signal_ret() {
    // it can be only called from syscall
    cpudata_t* cpdata = arch::x86_64::cpu::data();

    if(!cpdata)
        return;

    arch::x86_64::process_t* proc = cpdata->temp.proc;
    if(!proc)
        return;

    PREPARE_SIGNAL(proc) {

        proc->ctx = *proc->sys_ctx;
        proc->ctx.rax = EINTR;
        proc->ctx.rdx = 0;
        schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
        __builtin_unreachable();

    }
    
    return;

}

#define MAX_SYSCALL 100

extern "C" void* syscall_table[];

void* __syscall_find(int rax) {
    if(rax < MAX_SYSCALL && rax > 0)
        return syscall_table[rax];
    Log::SerialDisplay(LEVEL_MESSAGE_WARN,"unknown syscall %d",rax);
    return 0; // without it there will be ub
}

extern "C" void syscall_handler_c(int_frame_t* ctx) {

    void* item = __syscall_find(ctx->rax);

    if(!item) {
        return;
    } 

    cpudata_t* cpdata = arch::x86_64::cpu::data();
    arch::x86_64::process_t* proc = cpdata->temp.proc;
    proc->old_stack = ctx->rsp;

    if(ctx->rax != 10 && ctx->rax != 9 && ctx->rax != 11)
        proc->sys = ctx->rax;

    extern int last_sys;
    last_sys = ctx->rax;

    DEBUG(0,"sys %d from %d rip 0x%p",ctx->rax,proc->id,ctx->rip);
        
    proc->sys_ctx = ctx;

    syscall_ret_t (*sys)(std::uint64_t D, std::uint64_t S, std::uint64_t d, int_frame_t* frame) = (syscall_ret_t (*)(std::uint64_t, std::uint64_t, std::uint64_t, int_frame_t*))item;
    syscall_ret_t ret = sys(ctx->rdi,ctx->rsi,ctx->rdx,ctx);
    if(ret.is_rdx_ret) {
        ctx->rdx = ret.ret_val;
        if((std::int64_t)ret.ret_val < 0)
            DEBUG(proc->is_debug,"rdx ret 0x%p smaller than zero from sys %d",ret.ret_val,ctx->rax);
    }

    last_sys = 0;
    DEBUG(0,"sys ret %d from proc %d ",ret.ret,proc->id);

    if(ret.ret != 0 && ret.ret == EISDIR)
        DEBUG(1,"Syscall %d from proc %d fails with code %d",ctx->rax,proc->id,ret.ret);

    cpdata->temp.temp_ctx = 0;

    proc->sys_ctx = 0;

    ctx->rax = ret.ret; 
    return;
} 

void arch::x86_64::syscall::init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,(1 << 9)); // syscalls will enable interrupts when gs is swapped + stack is saved
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}

