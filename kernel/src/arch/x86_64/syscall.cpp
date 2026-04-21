#include <cstdint>
#include <arch/x86_64/cpu/idt.hpp>
#include <arch/x86_64/syscall.hpp>
#include <arch/x86_64/assembly.hpp>
#include <utils/assert.hpp>
#include <klibc/stdio.hpp>
#include <utils/errno.hpp>
#include <generic/userspace/syscall_list.hpp>

long long sys_stub() {
    return 0;
}

long long sys_enosys_stub() {
    return -ENOSYS;
}

extern "C" void syscall_handler();

syscall_item syscall_table[] = {
    {false, 1, (void*)sys_read},
    {false, 2, (void*)sys_write},
    {false, 3, (void*)sys_mmap},
    {false, 4, (void*)sys_munmap},
    {false, 5, (void*)sys_close},
    {false, 6, (void*)sys_futex},
    {false, 7, (void*)sys_exit_group},
    {false, 8, (void*)sys_exit},
    {false, 9, (void*)sys_arch_prctl},
    {false, 10, (void*)sys_seek},
    {false, 11, (void*)sys_ioctl},
    {false, 12, (void*)sys_access},
    {false, 13, (void*)sys_open},
    {false, 14, (void*)sys_openat},
    {false, 15, (void*)sys_unlink},
    {false, 16, (void*)sys_unlink_path},
    {false, 17, (void*)sys_newfstatat},
    {false, 18, (void*)sys_fstat},
    {false, 19, (void*)sys_statx},
    {false, 20, (void*)sys_pselect6},
    {false, 21, (void*)sys_poll},
    {false, 22, (void*)sys_dup},
    {false, 23, (void*)sys_dup2},
    {false, 24, (void*)sys_fcntl},
    {false, 25, (void*)sys_getpid},
    {false, 26, (void*)sys_gettid}, 
    {true, 27, (void*)sys_clone3},
    {false, 28, (void*)sys_clock_gettime},
    {false, 29, (void*)sys_getpgrp},
    {false, 30, (void*)sys_getppid},
    {false, 31, (void*)sys_setpgid},
    {false, 32, (void*)sys_getuid},
    {false, 33, (void*)sys_getresgid},
    {false, 34, (void*)sys_getresuid},
    {false, 35, (void*)sys_pipe2},
    {false, 36, (void*)sys_getrandom},
    {false, 37, (void*)sys_execve},
    {false, 38, (void*)sys_wait4},
    {false, 39, (void*)sys_writev},
    {false, 40, (void*)sys_readlink},
    {false, 41, (void*)sys_readlinkat},
    {false, 42, (void*)sys_getdents64},
    {false, 43, (void*)sys_statfs},
    {false, 44, (void*)sys_sigprocaction},
    {false, 45, (void*)sys_sigprocmask},
    {false, 46, (void*)sys_uname},
    {false, 47, (void*)sys_sigaltstack},
    {false, 48, (void*)sys_chmod},
    {false, 49, (void*)sys_chdir},
    {false, 50, (void*)sys_fchdir},
    {false, 51, (void*)sys_mkdir},
    {false, 52, (void*)sys_mkdirat},
    {false, 53, (void*)sys_umask},
    {false, 54, (void*)sys_faccessat2},
    {false, 55, (void*)sys_pread64},
    {false, 56, (void*)sys_prlimit64},
    {false, 57, (void*)sys_nanosleep},
    {false, 58, (void*)sys_yield},
    {true, 59, (void*)sys_clone},
    {true, 60, (void*)sys_newthread},
    {false, 61, (void*)sys_getpgid},
    {false, 62, (void*)sys_getgid},
    {false, 63, (void*)sys_ttyname},
    {false, 64, (void*)sys_sysinfo},
    {false, 65, (void*)sys_cpucount}
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

    if(current->is_debug)
        klibc::debug_printf("sys %d rdi 0x%p rsi 0x%p rdx 0x%p cwd %s\n", current_sys->num, ctx->rdi, ctx->rsi, ctx->rdx, current->cwd);

    if(current_sys->is_ctx_passed) {
        auto func = (long long (*)(x86_64::idt::int_frame_t*, long long, long long, long long, long long, long long, long long))(current_sys->sys);
        ret = func(ctx, ctx->rdi, ctx->rsi, ctx->rdx, ctx->r10, ctx->r8, ctx->r9);     
    } else {
        auto func = (long long (*)(long long, long long, long long, long long, long long, long long))(current_sys->sys);
        ret = func(ctx->rdi, ctx->rsi, ctx->rdx, ctx->r10, ctx->r8, ctx->r9);     
    }

    if(current->is_debug)
        klibc::debug_printf("sys %d ret %lli\n", current_sys->num, ret);

    assert(ctx->cr3 != 0, "uh nuh ");

    ctx->rax = ret;
    return;
}

void syscall::init() {
    assembly::wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    assembly::wrmsr(LSTAR,(uint64_t)syscall_handler);
    assembly::wrmsr(STAR_MASK,(1 << 9)); // syscalls will enable interrupts when gs is swapped + stack is saved
    assembly::wrmsr(EFER,assembly::rdmsr(EFER) | 1);
}