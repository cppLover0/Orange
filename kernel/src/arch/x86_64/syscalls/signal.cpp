
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
    return {0,ENOSYS,0};
    arch::x86_64::process_t* proc = CURRENT_PROC;

    DEBUG(proc->is_debug,"Setup default handler 0x%p from proc %d\n",handler,proc->id);

    return {0,0,0};
}

long long sys_pause() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    
    while(1) {
        signal_ret();
        yield();
    }

    return 0;
}

long long sys_tgkill(int tgid, int tid, int sig) {
    return sys_kill(tid,sig);
}

long long sys_kill(int pid, int sig) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    arch::x86_64::process_t* target_proc = arch::x86_64::scheduling::by_pid(pid);

    DEBUG(1,"Trying to kill %d with sig %d from proc %d\n",pid,sig,proc->id);

    if(!target_proc)
        return -ESRCH;

    if(pid == 0 && pid < 0)
        return 0; // not implemented

    if(!(proc->uid == 0 || proc->uid == target_proc->uid))
        return -EPERM;

    switch(sig) {
        case SIGKILL:
            target_proc->exit_code = 137;
            target_proc->_3rd_kill_lock.nowaitlock(); 
            break;
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
        case SIGSTOP:
            return 0;
        default:
            pending_signal_t pend_sig;
            pend_sig.sig = sig;
            target_proc->sig->push(&pend_sig);
            return 0;
    }
    return 0;
}

long long sys_sigaction(int signum, struct sigaction* hnd, struct sigaction* old, int_frame_t* ctx) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    SYSCALL_IS_SAFEZ(hnd,4096);
    SYSCALL_IS_SAFEZ(old,4096);

    DEBUG(proc->is_debug,"sigaction signum %d from proc %d with 0x%p",signum,proc->id,0);

    if(signum >= 36)
        return -EINVAL;

    void* old_hnd = proc->sig_handlers[signum];
    void* old_rest = proc->ret_handlers[signum];
    sigset_t* sigset = arch::x86_64::get_sigset_from_list(proc,signum);

    if(old) {
        memset(old,0,sizeof(struct sigaction));
        old->sa_handler = (void (*)(int))old_hnd;
        old->sa_restorer = (void (*)())old_rest;
        old->sa_flags = proc->sig_flags[signum];
        if(sigset) {
            memcpy(&old->sa_mask,sigset,sizeof(sigset_t));   
        }
    }

    if(hnd) {
        DEBUG(proc->is_debug,"sigaction %d, restorer 0x%p, handler 0x%p",signum,hnd->sa_restorer,hnd->sa_handler);
        proc->ret_handlers[signum] = (void*)hnd->sa_restorer;
        proc->sig_handlers[signum] = (void*)hnd->sa_handler;
        proc->sig_flags[signum] = hnd->sa_flags;
        if(!sigset) {
            auto siglist = (arch::x86_64::sigset_list*)memory::pmm::_virtual::alloc(sizeof(sigset_t));
            siglist->sig = signum;
            siglist->next = proc->sigset_list_obj;
            proc->sigset_list_obj = siglist;
            sigset = &siglist->sigset;
        }
        memcpy(sigset,&hnd->sa_mask,sizeof(sigset_t));
    }

    return 0;

}

#define SIG_BLOCK     0          /* Block signals. sigset_t *unewset 	size_t sigsetsize */
#define SIG_UNBLOCK   1          /* Unblock signals.  */
#define SIG_SETMASK   2          /* Set the set of blocked signals.  */

long long sys_alarm(int seconds) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    std::uint64_t t = proc->next_alarm;
    if(seconds != 0) {
        proc->next_alarm = drivers::tsc::currentus() + (seconds * (1000*1000));
        DEBUG(proc->is_debug,"alarm to %lli (%d seconds)",proc->next_alarm,seconds);
    } else {
        proc->next_alarm = 0;
    }
    if(t == 0) 
        return 0;
    else {
        t -= drivers::tsc::currentus();
        return (t / (1000 * 1000)) < 0 ? 0 : t / (1000 * 1000);
    }
    return 0;
}

long long sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, int_frame_t* ctx) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    SYSCALL_IS_SAFEZ((void*)set,4096);
    SYSCALL_IS_SAFEZ(oldset,4096);

    if(how != SIG_SETMASK)
        return 0; // unimplemented

    if(ctx->r10 != sizeof(sigset_t)) {
        DEBUG(proc->is_debug,"unsupported sigset len %d when normal is %d (even linux dont support this)",ctx->r10,sizeof(sigset_t));
        return 0;
    }

    // if(oldset) {
    //     memcpy(oldset,&proc->current_sigset,sizeof(sigset_t));
    // }

    if(set) {
        memcpy(&proc->current_sigset,set,sizeof(sigset_t));
    }

    return 0;
}

long long sys_setitimer(int which, itimerval* val, itimerval* old) {
    arch::x86_64::process_t* proc = CURRENT_PROC;

    SYSCALL_IS_SAFEZ(val,4096);
    SYSCALL_IS_SAFEZ(old,4096);

    if(!val)
        return -EINVAL;

    itimerval oldv = {0};
    std::uint64_t ns = 0;
    switch(which) {
    case ITIMER_REAL:
        oldv = proc->itimer;
        proc->itimer = *val;
        arch::x86_64::update_time(&proc->itimer,&proc->next_alarm,1);
        break;
    case ITIMER_PROF:
        oldv = proc->proftimer;
        proc->proftimer = *val;
        arch::x86_64::update_time(&proc->proftimer,&proc->prof_timer,0);
        break;
    case ITIMER_VIRTUAL:
        oldv = proc->vitimer;
        proc->vitimer = *val;
        arch::x86_64::update_time(&proc->vitimer,&proc->virt_timer,0);
        break;
    default:
        return -EINVAL;
    }

    if(old)
        *old = oldv;

    return 0;
}

long long sys_getitimer(int which, itimerval* val) {
    arch::x86_64::process_t* proc = CURRENT_PROC;

    SYSCALL_IS_SAFEZ(val,4096);

    if(!val)
        return -EINVAL;

    std::uint64_t ns = 0;
    switch(which) {
    case ITIMER_REAL:
        *val = proc->itimer;
        ns = (proc->next_alarm - drivers::tsc::currentus()) * 1000;
        val->it_value.tv_sec = ns / 1000000000;
        val->it_value.tv_usec = (ns % 1000000000) / 1000;
        return 0;
    case ITIMER_PROF:
        *val = proc->proftimer;
        ns = (proc->prof_timer) * 1000;
        val->it_value.tv_sec = ns / 1000000000;
        val->it_value.tv_usec = (ns % 1000000000) / 1000;
        return 0;
    case ITIMER_VIRTUAL:
        *val = proc->vitimer;
        ns = (proc->virt_timer) * 1000;
        val->it_value.tv_sec = ns / 1000000000;
        val->it_value.tv_usec = (ns % 1000000000) / 1000;
        return 0;
    default:
        return -EINVAL;
    }
    return -EINVAL;
}

// same as sys_pause but wait for signals
long long sys_sigsuspend(sigset_t* sigset, size_t size) {

    if(size != sizeof(sigset_t))
        return -EINVAL;

    if(!sigset)
        return -EINVAL;

    SYSCALL_IS_SAFEZ(sigset,4096);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    
    while(1) {
        signal_ret_sigmask(sigset);
        yield();
    }

    return 0;
}