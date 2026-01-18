
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/vmm.hpp>

#include <generic/vfs/fd.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <drivers/cmos.hpp>

#include <etc/libc.hpp>

#include <drivers/tsc.hpp>

#include <etc/errno.hpp>

#include <drivers/hpet.hpp>
#include <drivers/kvmtimer.hpp>
#include <generic/time.hpp>
#include <generic/vfs/vfs.hpp>

#include <etc/bootloaderinfo.hpp>
#include <generic/locks/spinlock.hpp>

syscall_ret_t sys_orangesigreturn(mcontext_t* mctx, int b, int c, int_frame_t* ctx) {
    // is_sig_real
    arch::x86_64::process_t* proc = CURRENT_PROC;
    mcontext_to_int_frame(mctx,&proc->ctx);
    proc->ctx.cs = 0x20 | 3;
    proc->ctx.ss = 0x18 | 3;
    proc->ctx.rflags |= (1 << 9);
    DEBUG(1,"endsig from proc %d rip 0x%p",proc->id,proc->ctx.rip);
    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
    __builtin_unreachable();
}

syscall_ret_t sys_orangedefaulthandler(void* handler) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->default_sig_handler = handler;

    DEBUG(proc->is_debug,"Setup default handler 0x%p from proc %d\n",handler,proc->id);

    return {0,0,0};
}

syscall_ret_t sys_pause() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    
    while(1) {
        signal_ret();
        yield();
    }

    return {0,0,0};
}


syscall_ret_t sys_kill(int pid, int sig) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    arch::x86_64::process_t* target_proc = arch::x86_64::scheduling::by_pid(pid);

    DEBUG(proc->is_debug,"Trying to kill %d with sig %d from proc %d\n",pid,sig,proc->id);

    if(!target_proc)
        return {0,ESRCH,0};

    if(pid == 0 && pid < 0)
        return {0,ENOSYS,0}; // not implemented

    if(!(proc->uid == 0 || proc->uid == target_proc->uid))
        return {0,EPERM,0};

    switch(sig) {
        case SIGKILL:
            target_proc->exit_code = 137;
            target_proc->_3rd_kill_lock.nowaitlock(); 
            break;
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
        case SIGSTOP:
            return {0,ENOSYS,0};
        default:
            pending_signal_t pend_sig;
            pend_sig.sig = sig;
            target_proc->sig->push(&pend_sig);
            return {0,0,0};
    }
    return {0,0,0};
}

syscall_ret_t sys_sigaction(int signum, struct sigaction* hnd, struct sigaction* old) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    SYSCALL_IS_SAFEA(hnd,4096);
    SYSCALL_IS_SAFEA(old,4096);

    DEBUG(proc->is_debug,"sigaction signum %d from proc %d with 0x%p",signum,proc->id,(void*)hnd->sa_handler);

    if(signum >= 31)
        return {0,0,0};

    void* old_hnd = proc->sig_handlers[signum];

    if(hnd) {
        proc->sig_handlers[signum] = (void*)hnd->sa_handler;
    }
    
    if(old) {
        memset(old,0,sizeof(struct sigaction));
        old->sa_handler = (void (*)(int))old_hnd;
    }

    return {0,0,0};

}