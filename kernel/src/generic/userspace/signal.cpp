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
#include <utils/signal_ret.hpp>
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

    klibc::debug_printf("sigaction sig %d restorer 0x%p handler 0x%p\n", sig, src ? src->sa_restorer : 0, src ? src->handler : 0);

    if(old) {
        old->handler = (void (*)(int))proc->signals_handlers[sig].handler;
        old->sa_restorer = (void (*)())proc->signals_handlers[sig].restorer;
        old->flags = proc->signals_handlers[sig].flags;
        old->sa_sigset = proc->signals_handlers[sig].sigset;
    }

    if(src) {
        proc->signals_handlers[sig].handler = (void*)src->handler;
        proc->signals_handlers[sig].restorer = (void*)src->sa_restorer;
        proc->signals_handlers[sig].flags = src->flags;
        proc->signals_handlers[sig].sigset = src->sa_sigset;
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

#if defined(__x86_64__)
#include <arch/x86_64/cpu/sse.hpp>
#endif

long long sys_sigreturn() {
    thread* current_thread = current_proc;

    signal_trace* current_trace = current_thread->sigtrace_obj;

    assert(current_trace, "meow ?");

    if(current_thread->is_debug) {
        klibc::debug_printf("sigreturn proc %d",current_thread->id);
    }

#if defined(__x86_64__)
    current_thread->should_not_save_ctx = true;
    current_thread->ctx = current_trace->ctx;
    klibc::memcpy(current_thread->sse_ctx, current_trace->sse_ctx, x86_64::sse::size());
    current_thread->sigset = current_trace->sigset;
    current_thread->sigtrace_obj = current_trace->next;
#else
#error "gfsf"
#endif
    process::yield();
    assert(0, ".");
    __builtin_unreachable();
}

long long sys_kill(int pid, int sig) {

    if(sig > 32)
        return -EINVAL;

    unsigned int stack_protect = 0xAB1C2DEA;
    thread* current = current_proc;
    

    if(pid == 0) {
        thread* head = process::_head_proc();
        while(head) {
            if(head->pid == current->pid && (head->status == PROCESS_LIVE || head->status == PROCESS_SLEEP)) {
                if(sig != 0 && head->id != 1) {
                    head->sig->push(sig);
                }
            }
            head = head->next;
        }

    } else if(pid == -1) {
        if(current->uid != 0)
            return -EPERM;

        thread* head = process::_head_proc();
        while(head) {
            // posix can kill current process but linux doesnt so i dont want to kill too
            if((head->status == PROCESS_LIVE || head->status == PROCESS_SLEEP) && head->id != 1 && head->id != current->id) {
                if(sig != 0) {
                    head->sig->push(sig);
                }
            }
            head = head->next;
        }

    } else if(pid < -1) {
        thread* head = process::_head_proc();
        while(head) {
            if((head->status == PROCESS_LIVE || head->status == PROCESS_SLEEP) && head->id != 1 && (int)head->pid == +pid) {
                if(sig != 0) {
                    head->sig->push(sig);
                }
            }
            head = head->next;
        }
    } else if(pid > 0) {
        volatile thread* target = process::by_id(pid);

        if(target == nullptr)
            return -ESRCH;

        if(current->is_debug) {
            klibc::debug_printf("killing proc %d %d with sig %d from %d\n", target->id, pid, sig, current->id);
        }

        
        assert(stack_protect == 0xAB1C2DEA, "stack is fucked :(");
        assert(current != target, "1 %d %d %d 0x%p 0x%p", current->id, target->id, current->id != target->id, stack_protect, 0xAB1C2DEA);
        assert(current->sig != target->sig, "man");

        if(target->id != 1 && (target->status == PROCESS_LIVE || target->status == PROCESS_SLEEP)) {
            if(sig != 0) {
                target->sig->push(sig);
            }
        }
    } else {
        assert(0, "unimplemented kill %d with sig %d", pid, sig);
        return -EFAULT;
    }

    is_signal_ret(current) {
        signal_ret(current);
        __builtin_unreachable();
    }

    return 0;
}

long long sys_pause() {
    thread* proc = current_proc;
    while(1) {
        is_signal_ret(proc) {
            signal_ret(proc);
        }
    }
    assert(0, "pause ssss");
    return -EFAULT;
}