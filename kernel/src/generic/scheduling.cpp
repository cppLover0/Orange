#include <cstdint>
#include <generic/scheduling.hpp>
#include <generic/arch.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/hhdm.hpp>
#include <utils/gobject.hpp>
#include <generic/mp.hpp>
#include <utils/assert.hpp>
#include <generic/vfs.hpp>
#include <generic/time.hpp>
#include <utils/stack.hpp>
#include <utils/signal.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/cpu_local.hpp>
#include <arch/x86_64/cpu/sse.hpp>
#endif

#define KERNEL_STACK_SIZE (1024 * 128)

std::atomic<void*> head_proc = nullptr;

locks::preempt_spinlock scheduling_lock;
std::uint32_t last_id = 0;

std::uint32_t proc_count = 0;

void scheduling_balance_cpus() {
    std::uint32_t cpu_ptr = 0;
    thread* proc = (thread*)head_proc.load();
    while(proc) {
        if(proc->status != PROCESS_KILLED && proc->status != PROCESS_ZOMBIE) {
            proc->cpu = cpu_ptr++;
            if(cpu_ptr == mp::cpu_count()) cpu_ptr = 0;
        }
        proc = proc->next;
    }
}


thread* process::create_process(bool is_user) {
    thread* new_thread = (thread*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_thread->id = ++last_id;
    new_thread->status = PROCESS_SLEEP;
    new_thread->lock.lock();
    new_thread->pid = new_thread->id;
    new_thread->original_root = pmm::freelist::alloc_4k();

#if defined(__x86_64__)
    new_thread->sse_ctx = (std::uint8_t*)(pmm::buddy::alloc(x86_64::sse::size()).phys + etc::hhdm());
    x86_64::sse::setup_headers((x86_64::sse::fpu_head_t*)new_thread->sse_ctx); // yes ik but why not 
    new_thread->ctx.ss = is_user ? (0x18 | 3) : 0;
    new_thread->ctx.cs = is_user ? (0x20 | 3) : 0x08;
    new_thread->ctx.rflags = (1 << 9);
    new_thread->ctx.cr3 = new_thread->original_root;
#endif

    new_thread->is_debug = true;
    new_thread->sig = new signal_manager;
    new_thread->original_syscall_stack = pmm::buddy::alloc(KERNEL_STACK_SIZE).phys;
    new_thread->syscall_stack = (std::uint64_t)(new_thread->original_syscall_stack + etc::hhdm() + (KERNEL_STACK_SIZE - PAGE_SIZE));
    new_thread->cwd = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_thread->cwd[0] = '/';
    new_thread->cwd[1] = '\0';

    proc_count++;

    bool state = scheduling_lock.lock();
    new_thread->next = (thread*)head_proc.load();
    head_proc = (void*)new_thread;

    scheduling_balance_cpus();
    scheduling_lock.unlock(state);

    return new_thread;
}

thread* process::clone3(thread* proc, clone_args clarg, void* frame) {
    thread* new_proc = create_process(true);

    klibc::memcpy(&new_proc->ctx, frame, sizeof(proc->ctx));
    if(clarg.flags & CLONE_THREAD) {
        new_proc->pid = proc->pid;
        new_proc->pgrp = proc->pgrp;
    } else {
        new_proc->pid = new_proc->id;
        new_proc->pgrp = new_proc->id;
    }

    if(clarg.flags & CLONE_PARENT) {
        new_proc->parent_pid = proc->parent_pid;
    } else {
        new_proc->parent_pid = proc->id;
    }

    if(clarg.flags & CLONE_VM) {
        pmm::freelist::free(new_proc->original_root);
        new_proc->original_root = proc->original_root;
#if defined(__x86_64__)
        new_proc->ctx.cr3 = proc->original_root;
#endif
        new_proc->vmem = proc->vmem;
        proc->vmem->new_usage();
    } else {
#if defined(__x86_64__)
        new_proc->ctx.cr3 = new_proc->original_root;
#endif
        new_proc->vmem = new vmm;
        new_proc->vmem->root = new_proc->original_root;
        proc->vmem->duplicate(new_proc->vmem);
        new_proc->vmem->init_root();
    }

    new_proc->uid = proc->uid;
    new_proc->exit_signal = clarg.exit_signal;

#if defined(__x86_64__)
    if(clarg.flags & CLONE_SETTLS) {
        new_proc->fs_base = clarg.tls;
    } else {
        new_proc->fs_base = proc->fs_base;
    }
#endif

#if defined(__x86_64__)
    if(clarg.stack) {
        new_proc->ctx.rsp = clarg.stack + clarg.stack_size;
    } 
#endif

    klibc::memcpy(new_proc->signals_handlers, proc->signals_handlers, sizeof(proc->signals_handlers));
    if(proc->chroot) {
        new_proc->chroot = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
        klibc::memcpy(new_proc->chroot, proc->chroot, klibc::strlen(proc->chroot) + 1);
    }

    if(proc->cwd) {
        klibc::memcpy(new_proc->cwd, proc->cwd, klibc::strlen(proc->cwd) + 1);
    }

    if(proc->name) {
        new_proc->name = (char*)(pmm::freelist::alloc_4k() + etc::hhdm());
        klibc::memcpy(new_proc->name, proc->name, klibc::strlen(proc->name) + 1);
    }

#if defined(__x86_64__)
    if(proc->sse_ctx) {
        klibc::memcpy(new_proc->sse_ctx, proc->sse_ctx, x86_64::sse::size());
    }
#endif

    if(clarg.flags & CLONE_FILES) {
        vfs::fdmanager* manager = (vfs::fdmanager*)proc->fd;
        new_proc->fd = manager;
        manager->new_usage();
    } else {
        vfs::fdmanager* manager = new vfs::fdmanager;
        new_proc->fd = (void*)manager;
        vfs::fdmanager* src_manager = (vfs::fdmanager*)proc->fd;
        src_manager->duplicate(manager);
        manager->fd_usage_pointer = 1;
    }

    if(clarg.flags & CLONE_PARENT_SETTID) {
        *(int*)clarg.parent_tid = new_proc->id;
    }

    if(clarg.flags & CLONE_CHILD_SETTID) {
        new_proc->pending_child_settid = (int*)clarg.child_tid;
    }

    assert(new_proc->ctx.cr3, "type shit");
    assert(new_proc->original_root, "type shit");
    assert(new_proc->sig != proc->sig, "oh man");

    return new_proc;

}

thread* process::by_id(std::uint32_t id) {
    thread* current = (thread*)head_proc.load();
    while(current) {
        if(current->id == id) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

thread* process::kthread(void (*func)(void*), void* arg) {
    thread* new_thread = (thread*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_thread->id = ++last_id;
    new_thread->status = PROCESS_SLEEP;
    new_thread->lock.lock();
    new_thread->pid = new_thread->id;
#if defined(__x86_64__)
    new_thread->sse_ctx = (std::uint8_t*)(pmm::buddy::alloc(x86_64::sse::size()).phys + etc::hhdm());
    x86_64::sse::setup_headers((x86_64::sse::fpu_head_t*)new_thread->sse_ctx); // yes ik but why not 
    new_thread->ctx.ss = 0;
    new_thread->ctx.cs = 0x08;
    new_thread->ctx.rip = (std::uint64_t)func;
    new_thread->ctx.rdi = (std::uint64_t)arg;
    new_thread->ctx.rsp = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm() + (KERNEL_STACK_SIZE - PAGE_SIZE));
    new_thread->ctx.cr3 = gobject::kernel_root;
    new_thread->ctx.rflags = 0;
#endif
    new_thread->original_root = gobject::kernel_root;

    bool state = scheduling_lock.lock();
    new_thread->next = (thread*)head_proc.load();
    head_proc = (void*)new_thread;
    scheduling_balance_cpus();
    scheduling_lock.unlock(state);

    return new_thread;
}

void process::wakeup(thread* thread) {
    thread->status = PROCESS_LIVE;
    thread->lock.unlock();
}

int process::futex_wake(thread* proc, int* lock, int count) {
    thread* current = (thread*)(head_proc.load());
    int c = 0;

    if(count == 0)
        return 0;

    while(current) {
        if(current->vmem == proc->vmem && current->futex.load() == (std::uint64_t)lock) {
            current->futex.store(0);
            if(current->is_debug) {
                klibc::debug_printf("futex wake proc %d from proc %d futex 0x%p count %d c %d\n", current->id, proc->id, lock, count, c);
            }
            c++;
        }

        if(c == count) 
            break;
        current = current->next;
    }
    return c;
}

void process::futex_wait(thread* proc, int* lock) {
    if(proc->is_debug) {
        klibc::debug_printf("futex wait proc %d lock 0x%p\n", proc->id, lock);
    }
    proc->futex.store((std::uint64_t)lock);
}

void process::kill(thread* t, int status, bool exit_group) {
    auto exit = [status](thread* t) {

        if(t->robust) {
            arch::enable_paging(t->original_root);

            robust_list* current = t->robust->list.next;
            while(current) {

                int* futex = *(int**)(current + t->robust->futex_offset);

                futex_wake(t, futex, 99999999);

                current = current->next;
            
                if((std::uint64_t)current == (std::uint64_t)t->robust)
                    break;

            }

            arch::enable_paging(gobject::kernel_root);
        }

        t->status = PROCESS_ZOMBIE;
        t->lock.try_lock();
        if(t->syscall_stack) pmm::buddy::free(t->original_syscall_stack - etc::hhdm());
        if(t->name) pmm::freelist::free((std::uint64_t)t->name - etc::hhdm());
        if(t->chroot) pmm::freelist::free((std::uint64_t)t->chroot - etc::hhdm());
        if(t->cwd) pmm::freelist::free((std::uint64_t)t->cwd - etc::hhdm());
        if(t->sig) delete t->sig;
        t->syscall_stack = 0;
        t->name = 0;
        t->chroot = 0;
        t->cwd = 0;
        t->sig = 0;
        t->vmem->free();

        t->did_exec = true;
        vfs::fdmanager* manager = (vfs::fdmanager*)t->fd;
        manager->kill();

        proc_count--;
        t->vmem = 0;
        t->fd = 0;
        t->exit_code = status;

    };
    
    if(exit_group) {
        thread* current = (thread*)head_proc.load();
        int target_pid = t->pid;
        while(current) {
            if(current->pid == (std::uint32_t)target_pid && (current->status == PROCESS_SLEEP || current->status == PROCESS_LIVE)) {
                current->exit_request = 1;
                if(current == t)
                    exit(current);
            }
            current = current->next;
        }
    } else {
        exit(t);
    }
}

void process::schedule(void* ctx) {
    thread* current_thread = nullptr;
    std::uint32_t current_cpu = 0;
    assert(ctx,"omg !341(182)");
#if defined(__x86_64__)
    current_thread = (thread*)CPU_LOCAL_READ(current_thread);
    current_cpu = (std::uint32_t)CPU_LOCAL_READ(cpu);

#endif

    if(current_cpu == 0)
        time::update_unix_time();

    // save context
    if(current_thread) {
        if(ctx && !current_thread->should_not_save_ctx) {
#if defined(__x86_64__)
            klibc::memcpy(&current_thread->ctx, ctx, sizeof(current_thread->ctx));
            if(current_thread->sse_ctx)
                x86_64::sse::save(current_thread->sse_ctx);
            current_thread->user_stack = (std::uint64_t)CPU_LOCAL_READ(user_stack);
#else
#error MEOW MEOW MEOw
#endif

        }
        current_thread->should_not_save_ctx = false;
        current_thread->lock.unlock();
        current_thread = current_thread->next;
    } else {
        current_thread = (thread*)head_proc.load();
    }

    while(true) {
        while(current_thread) {
           // klibc::printf("current_proc %d cpu %d lock %d status %d\r\n",current_thread->id,current_thread->cpu.load(), current_thread->lock.test(), current_thread->status.load());
            
           if(current_thread->cpu == current_cpu && current_thread->status == PROCESS_LIVE) {
                if(!current_thread->lock.try_lock()) {

                    if(current_thread->exit_request != 0) {
                        kill(current_thread, current_thread->exit_code, current_thread->exit_request == 2 ? true : false);
                        goto happy;
                    }

// signals for now only for x86_64 at least cuz i have no idea how this shit works on other arches
#if defined(__x86_64__)
                    if(current_thread->sig && current_thread->ctx.cs != 0x08) {
                        std::int8_t sig = current_thread->sig->pop(&current_thread->sigset);
                        if(sig != -1) {
                            if(sig != SIGKILL) {
                                if(current_thread->signals_handlers[sig].handler == (void*)SIG_DFL) {
                                    switch(sig) {
                                        case SIGCHLD:
                                        case SIGCONT:
                                        case SIGURG:
                                        case SIGWINCH:
                                            break; //ignore
                                        case SIGSTOP:
                                        case SIGTSTP:
                                        case SIGTTIN:
                                        case SIGTTOU:
                                            log("scheduling", "unimplemented process stop sig %d proc %d", sig, current_thread->id);
                                            break;
                                        default: {
                                            kill(current_thread, 128 + sig, true);
                                            goto happy;
                                        }
                                    }
                                } else if(current_thread->signals_handlers[sig].handler != (void*)SIG_IGN) {
                                    if(current_thread->is_restore_sigset) {
                                        // syscall requested restore sigset
                                        klibc::memcpy(&current_thread->sigset,&current_thread->temp_sigset,sizeof(sigset_t));
                                        current_thread->is_restore_sigset = 0;
                                    }

                                    if(current_thread->is_debug) {
                                        klibc::debug_printf("AAAA sig %d to proc %d 0x%p 0x%p\n", sig, current_thread->id, current_thread->signals_handlers[sig].handler, current_thread->signals_handlers[sig].restorer);
                                    }

                                    signal_trace new_sigtrace;

                                    arch::enable_paging(current_thread->original_root);

                                    std::uint64_t* new_stack = 0;

                                    if(!current_thread->sigtrace_obj) {
                                        new_stack = (std::uint64_t*)(!(current_thread->signals_handlers[sig].flags & SA_ONSTACK) ? current_thread->ctx.rsp - 8 : (std::uint64_t)current_thread->alt_stack.ss_sp);
                                    } else {
                                        new_stack = (std::uint64_t*)(current_thread->ctx.rsp - 8);
                                    }

                                    new_stack = (std::uint64_t*)ALIGNDOWN(((std::uint64_t)new_stack - PAGE_SIZE),16);

                                    new_stack = (std::uint64_t*)stackmgr::memcpy((std::uint64_t)new_stack,current_thread->sse_ctx,x86_64::sse::size());

                                    new_sigtrace.sse_ctx = (std::uint8_t*)new_stack;

                                    new_sigtrace.next = current_thread->sigtrace_obj;
                                    new_sigtrace.sigset = current_thread->sigset;
                                    new_sigtrace.ctx = current_thread->ctx;

                                    new_stack = (std::uint64_t*)stackmgr::memcpy((std::uint64_t)new_stack,&new_sigtrace,sizeof(new_sigtrace));
                                    signal_trace* new_sigtrace_stack = (signal_trace*)new_stack;

                                    current_thread->sigtrace_obj = new_sigtrace_stack;

                                    --new_stack;

                                    new_stack = (uint64_t*)ALIGNDOWN((uint64_t)new_stack, 16);
                                    *--new_stack = (std::uint64_t)current_thread->signals_handlers[sig].restorer;

                                    // // now setup new registers and stack
                                    current_thread->ctx = {};
                                    current_thread->ctx.rdi = (std::uint64_t)sig;
                                    current_thread->ctx.rsi = 0;
                                    current_thread->ctx.rdx = 0;
                                    current_thread->ctx.rsp = (std::uint64_t)new_stack;
                                    current_thread->ctx.rip = (std::uint64_t)current_thread->signals_handlers[sig].handler;
                                    current_thread->ctx.cs = 0x20 | 3;
                                    current_thread->ctx.ss = 0x18 | 3;
                                    current_thread->ctx.cr3 = current_thread->original_root;
                                    current_thread->ctx.rflags = (1 << 9);

                                    arch::enable_paging(gobject::kernel_root);

                                }
                            } else {
                                kill(current_thread, 128 + SIGKILL, true);
                                goto happy;
                            }
                        }
                    }
#endif

                    klibc::memcpy(ctx, &current_thread->ctx, sizeof(current_thread->ctx));
#if defined(__x86_64__)
                    assembly::wrmsr(0xC0000100, current_thread->fs_base);
                    if(current_thread->sse_ctx)
                        x86_64::sse::load(current_thread->sse_ctx);
                    CPU_LOCAL_WRITE(kernel_stack, current_thread->syscall_stack);
                    CPU_LOCAL_WRITE(user_stack, current_thread->user_stack);
                    CPU_LOCAL_WRITE(current_thread, current_thread);

#endif

                    if(current_thread->pending_child_settid != nullptr) {
                        arch::enable_paging(current_thread->original_root);
                        *current_thread->pending_child_settid = current_thread->id;
                        if(current_thread->is_debug) {
                            klibc::debug_printf("setting settid 0x%p with %d (bullshit)\n", current_thread->pending_child_settid, current_thread->id);
                        }
                        current_thread->pending_child_settid = nullptr;
                        arch::enable_paging(gobject::kernel_root);
                    }

                    assert(current_thread->original_root == current_thread->ctx.cr3, ":(c");

                    return;
                }
            } 

happy:
            current_thread = current_thread->next;
        }

        arch::pause();
        current_thread = (thread*)head_proc.load();
    }

}

thread* process::_head_proc() {
    return (thread*)head_proc.load();
}