#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <utils/linux.hpp>
#include <generic/userspace/robust.hpp>
#include <generic/userspace/safety.hpp>
#include <utils/random.hpp>
#include <utils/signal.hpp>

long long sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, std::uint64_t len) {
    thread* proc = current_proc;
    
    if(!is_safe_to_rw(proc, (std::uint64_t)set, sizeof(sigset_t))) {
        return -EFAULT;
    }

    if(!is_safe_to_rw(proc, (std::uint64_t)oldset, sizeof(sigset_t))) {
        return -EFAULT;
    }

    if(len != sizeof(sigset_t)) {
        assert(0,"n %d", len);
        return -EINVAL;
    }

    if(proc->is_debug) {
        klibc::debug_printf("sigprocmask how %d 0x%p 0x%p %lli", how, set, oldset, len);
    }

    if(oldset) {
        klibc::memcpy(oldset,&proc->sigset,sizeof(sigset_t));
    }

    if(set) {
        if(how == SIG_BLOCK) {
            proc->sigset.__val |= set->__val;
        } else if(how == SIG_UNBLOCK) {
            proc->sigset.__val &= ~(set->__val);
        } else if(how == SIG_SETMASK) {
            proc->sigset = *set;
        }
    }


    return 0;
}

long long sys_sigprocaction(int sig, const sigaction* src, sigaction* old, std::uint64_t len) {
    (void)len;
    assert(sig < 34, "m %d", sig);

    thread* proc = current_proc;
    if(!is_safe_to_rw(proc, (std::uint64_t)src, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)old, PAGE_SIZE))
        return -EFAULT;


    if(old) {
        old->__sigaction_handler.sa_handler = (void (*)(int))proc->signals_handlers[sig].handler;
        old->sa_restorer = (void (*)())proc->signals_handlers[sig].restorer;
        old->sa_flags = proc->signals_handlers[sig].flags;
        old->sa_mask = proc->signals_handlers[sig].sigset;
    }

    if(src) {
        proc->signals_handlers[sig].handler = (void*)src->__sigaction_handler.sa_handler;
        proc->signals_handlers[sig].restorer = (void*)src->sa_restorer;
        proc->signals_handlers[sig].flags = src->sa_flags;
        proc->signals_handlers[sig].sigset = src->sa_mask;
    }

    return 0;
}

long long sys_sigaltstack(const sig_stack* stack, sig_stack* old) {
    thread* current_thread = current_proc;
    if(!is_safe_to_rw(current_thread, (std::uint64_t)stack, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(current_thread, (std::uint64_t)old, PAGE_SIZE))
        return -EFAULT;

    if(old) {
        *old = current_thread->signal_stack;
    }

    if(stack) {
        current_thread->signal_stack = *stack;
    }

    return 0;
}