#include <cstdint>
#include <generic/scheduling.hpp>
#include <generic/arch.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/hhdm.hpp>
#include <utils/gobject.hpp>
#include <generic/mp.hpp>
#include <utils/assert.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/cpu_local.hpp>
#include <arch/x86_64/cpu/sse.hpp>
#endif

#define KERNEL_STACK_SIZE (1024 * 32)

std::atomic<void*> head_proc = nullptr;

locks::preempt_spinlock scheduling_lock;
std::uint32_t last_id = 0;

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

    bool state = scheduling_lock.lock();
    new_thread->next = (thread*)head_proc.load();
    head_proc = (void*)new_thread;
    scheduling_lock.unlock(state);

    return new_thread;
}

thread* process::by_id(std::uint32_t id) {
    thread* current = (thread*)head_proc.load();
    while(current) {
        if(current->id == id)
            return current;
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
    scheduling_lock.unlock(state);

    return new_thread;
}

void process::wakeup(thread* thread) {
    thread->status = PROCESS_LIVE;
    thread->lock.unlock();
}

void process::kill(thread* thread) {
    thread->status = PROCESS_ZOMBIE;
    thread->lock.try_lock();
    if(thread->syscall_stack) pmm::buddy::free(thread->syscall_stack - etc::hhdm());
    if(thread->name) pmm::freelist::free((std::uint64_t)thread->name - etc::hhdm());
    if(thread->chroot) pmm::freelist::free((std::uint64_t)thread->name - etc::hhdm());
    if(thread->cwd) pmm::freelist::free((std::uint64_t)thread->name - etc::hhdm());
    if(thread->sig) delete thread->sig;
    thread->syscall_stack = 0;
    thread->name = 0;
    thread->chroot = 0;
    thread->cwd = 0;
    thread->sig = 0;
}

void process::schedule(void* ctx) {
    thread* current_thread = nullptr;
    std::uint32_t current_cpu = 0;
    assert(ctx,"omg !341(182)");
#if defined(__x86_64__)
    current_thread = (thread*)CPU_LOCAL_READ(current_thread);
    current_cpu = (std::uint32_t)CPU_LOCAL_READ(cpu);
#endif

    // save context
    if(current_thread) {
        if(ctx) {
#if defined(__x86_64__)
            klibc::memcpy(&current_thread->ctx, ctx, sizeof(current_thread->ctx));
            if(current_thread->sse_ctx)
                x86_64::sse::save(current_thread->sse_ctx);
            current_thread->user_stack = (std::uint64_t)CPU_LOCAL_READ(user_stack);
#endif
        }
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
                    klibc::memcpy(ctx, &current_thread->ctx, sizeof(current_thread->ctx));
#if defined(__x86_64__)
                    assembly::wrmsr(0xC0000100, current_thread->fs_base);
                    if(current_thread->sse_ctx)
                        x86_64::sse::load(current_thread->sse_ctx);
                    CPU_LOCAL_WRITE(kernel_stack, current_thread->syscall_stack);
                    CPU_LOCAL_WRITE(user_stack, current_thread->user_stack);
                    CPU_LOCAL_WRITE(current_thread, current_thread);

#endif
                    return;
                }
            } 
            current_thread = current_thread->next;
        }

        arch::pause();
        current_thread = (thread*)head_proc.load();
    }

}