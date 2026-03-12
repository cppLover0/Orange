#include <cstdint>
#include <generic/scheduling.hpp>
#include <generic/arch.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/hhdm.hpp>
#include <utils/gobject.hpp>
#include <generic/mp.hpp>
#define KERNEL_STACK_SIZE (1024 * 32)

thread* head_proc = 0;
std::uint32_t last_id = 0;

void scheduling_balance_cpus() {
    std::uint32_t cpu_ptr = 0;
    thread* proc = head_proc;
    while(proc) {
        if(proc->status != PROCESS_KILLED && proc->status != PROCESS_ZOMBIE) {
            proc->cpu = cpu_ptr++;
            if(cpu_ptr == mp::cpu_count()) cpu_ptr = 0;
        }
        proc = proc->next;
    }
}


thread* process::create_process(bool is_user) {
    (void)is_user;
    return nullptr;
}

thread* process::by_id(std::uint32_t id) {
    thread* current = head_proc;
    while(current) {
        if(current->id == id)
            return current;
    }
    return nullptr;
}

thread* process::kthread(void (*func)(void*), void* arg) {
    thread* new_thread = (thread*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_thread->id = ++last_id;
    new_thread->lock.lock();
    new_thread->pid = new_thread->id;
#if defined(__x86_64__)
    new_thread->ctx.ss = 0;
    new_thread->ctx.cs = 0x08;
    new_thread->ctx.rip = (std::uint64_t)func;
    new_thread->ctx.rdi = (std::uint64_t)arg;
    new_thread->ctx.rsp = (std::uint64_t)(pmm::buddy::alloc(KERNEL_STACK_SIZE).phys + etc::hhdm() + (KERNEL_STACK_SIZE - 1024));
    new_thread->ctx.cr3 = gobject::kernel_root;
#endif
    new_thread->original_root = gobject::kernel_root;
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
    (void)ctx;
}