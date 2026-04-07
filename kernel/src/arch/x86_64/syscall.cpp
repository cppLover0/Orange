#include <cstdint>
#include <arch/x86_64/cpu/idt.hpp>
#include <arch/x86_64/syscall.hpp>
#include <arch/x86_64/assembly.hpp>
#include <utils/assert.hpp>
#include <klibc/stdio.hpp>
#include <utils/errno.hpp>
#include <generic/userspace/syscall_list.hpp>

long long sys_stub() {
    return -ENOSYS;
}

extern "C" void syscall_handler();

syscall_item syscall_table[] = {
    {false, 12, (void*)sys_brk}, // implement brk todo 
    {false, 21, (void*)sys_access},
    {false, 257, (void*)sys_openat},
    {false, 262, (void*)sys_newfstatat},
    {false, 0, (void*)sys_read},
    {false, 5, (void*)sys_fstat},
    {false, 9, (void*)sys_mmap},
    {false, 3, (void*)sys_close},
    {false, 17, (void*)sys_pread64},
    {false, 158, (void*)sys_arch_prctl},
    {false, 218, (void*)sys_set_tid_address}
};

syscall_item* find_syscall(long long num) {
    for(uint64_t i = 0; i < sizeof(syscall_table) / sizeof(syscall_item);i++) {
        if(syscall_table[i].num == num)
            return &syscall_table[i];
    }
    return nullptr;
}

extern "C" void syscall_handler_c(x86_64::idt::int_frame_t* ctx) {
    syscall_item* current_sys = find_syscall(ctx->rax);
    if(current_sys == nullptr) {
        assert(0, "unimplemented syscall %lli",ctx->rax);
    }

    thread* current = current_proc;

    long long ret = 0;

    if(current_sys->is_ctx_passed) {
        auto func = (long long (*)(x86_64::idt::int_frame_t*, long long, long long, long long, long long, long long, long long))(current_sys->sys);
        ret = func(ctx, ctx->rdi, ctx->rsi, ctx->rdx, ctx->r10, ctx->r8, ctx->r9);     
    } else {
        auto func = (long long (*)(long long, long long, long long, long long, long long, long long))(current_sys->sys);
        ret = func(ctx->rdi, ctx->rsi, ctx->rdx, ctx->r10, ctx->r8, ctx->r9);     
    }

    if(current->is_debug)
        klibc::debug_printf("sys %d ret %lli\n", current_sys->num, ret);

    ctx->rax = ret;
    return;
}

void syscall::init() {
    assembly::wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    assembly::wrmsr(LSTAR,(uint64_t)syscall_handler);
    assembly::wrmsr(STAR_MASK,(1 << 9)); // syscalls will enable interrupts when gs is swapped + stack is saved
    assembly::wrmsr(EFER,assembly::rdmsr(EFER) | 1);
}