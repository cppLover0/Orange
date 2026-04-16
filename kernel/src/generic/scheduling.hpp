#pragma once
#include <cstdint>
#if defined(__x86_64__)
#include <arch/x86_64/cpu/idt.hpp>
#elif defined(__aarch64__)
#include <arch/aarch64/cpu/el.hpp>
#endif
#include <utils/signal.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/userspace/robust.hpp>
#include <utils/linux.hpp>
#include <generic/vmm.hpp>
#include <atomic>

#define PROCESS_NONE 1
#define PROCESS_LIVE 2
#define PROCESS_KILLED 3
#define PROCESS_ZOMBIE 4
#define PROCESS_SLEEP 5

struct signal_member {
    void* restorer;
    void* handler;
    std::uint32_t flags;
    sigset_t sigset;
};

struct signal_trace {
#if defined(__x86_64__)
    x86_64::idt::int_frame_t ctx;
    std::uint8_t* sse_ctx;
#elif defined(__aarch64__)
    aarch64::el::int_frame ctx;
#endif
    sigset_t sigset;
    signal_trace* next;
};

struct thread {
    std::uint32_t id;
    std::uint32_t pid;
    std::uint32_t parent_pid;
    std::uint32_t pgrp;
    std::uint32_t exit_code;
    std::uint32_t exit_signal;
    std::uint32_t waitpid_state;
    bool should_not_save_ctx;
#if defined(__x86_64__)
    std::uint8_t* sse_ctx;
    std::uint64_t fs_base;
    x86_64::idt::int_frame_t ctx;
    x86_64::idt::int_frame_t signal_ctx; // should be filled with syscalls
#elif defined(__aarch64__)
    aarch64::el::int_frame ctx;
    aarch64::el::int_frame signal_ctx;
#endif
    sig_stack alt_stack;
    signal_trace* sigtrace_obj;
    signal_manager* sig;
    signal_member signals_handlers[34];
    sig_stack signal_stack;
    locks::spinlock lock;
    std::atomic<std::uint32_t> futex;
    std::atomic<std::uint32_t> status;
    std::atomic<std::uint32_t> cpu;
    std::uint64_t original_syscall_stack;
    std::uint64_t syscall_stack;
    std::uint64_t user_stack;
    std::uint64_t original_root;
    bool is_debug;
    bool is_restore_sigset;
    int exit_request; // 1 signle exit, 2 group exit
    std::atomic<bool> did_exec;

    std::uint64_t ts;
    int* pending_child_settid;
    sigset_t temp_sigset;

    robust_list_head* robust;
    int* tidptr;
    std::atomic<int> fd_ptr;
 
    int uid;
    int gid;
    
    void* fd;
    vmm* vmem;

    char* name;
    char* cwd;
    char* chroot;

    sigset_t sigset;
    thread* next;
};

static_assert(sizeof(thread) < 4096, "thread struct is bigger than page size (bug)");

namespace process {
    thread* create_process(bool is_user);
    thread* by_id(std::uint32_t id);
    thread* kthread(void (*func)(void*), void* arg);

    thread* clone3(thread* proc, clone_args clarg, void* frame);
    void wakeup(thread* thread);
    void kill(thread* thread, int status, bool exit_group);

    int futex_wake(thread* proc, int* lock, int count);
    void futex_wait(thread* proc, int* lock);

    extern "C" void schedule(void* frame);

    extern "C" void switch_ctx(void* frame);
    extern "C" void yield();

    thread* _head_proc();
};