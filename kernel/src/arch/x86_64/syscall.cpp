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
    {false, 12, (void*)sys_brk}, // implement brk todo (no)
    {false, 21, (void*)sys_access},
    {false, 257, (void*)sys_openat},
    {false, 262, (void*)sys_newfstatat},
    {false, 0, (void*)sys_read},
    {false, 5, (void*)sys_fstat},
    {false, 9, (void*)sys_mmap},
    {false, 3, (void*)sys_close},
    {false, 17, (void*)sys_pread64},
    {false, 158, (void*)sys_arch_prctl},
    {false, 218, (void*)sys_set_tid_address},
    {false, 273, (void*)sys_set_robust_list},
    {false, 274, (void*)sys_get_robust_list}, 
    {false, 334, (void*)sys_rseq}, // todo
    {false, 10, (void*)sys_stub}, // i dont want to support mprotect
    {false, 302, (void*)sys_prlimit64},
    {false, 16, (void*)sys_ioctl},
    {false, 318, (void*)sys_getrandom},
    {false, 89, (void*)sys_readlink},
    {false, 267, (void*)sys_readlinkat},
    {false, 102, (void*)sys_getuid},
    {false, 104, (void*)sys_getgid},
    {false, 107, (void*)sys_getuid},
    {false, 108, (void*)sys_getgid}, // getegid
    {false, 14, (void*)sys_sigprocmask},
    {false, 96, (void*)sys_gettimeofday},
    {false, 13, (void*)sys_sigprocaction},
    {false, 63, (void*)sys_uname},
    {false, 41, (void*)sys_enosys_stub}, // todo do sockets
    {false, 79, (void*)sys_getcwd},
    {false, 39, (void*)sys_getpid},
    {false, 110, (void*)sys_getppid},
    {false, 111, (void*)sys_getpgrp},
    {false, 32, (void*)sys_dup},
    {false, 72, (void*)sys_fcntl},
    {false, 33, (void*)sys_dup2},
    {false, 109, (void*)sys_setpgid},
    {false, 439, (void*)sys_faccessat2},
    {false, 293, (void*)sys_pipe2},
    {true, 56, (void*)sys_clone},
    {false, 1, (void*)sys_write},
    {false, 201, (void*)sys_time},
    {false, 7, (void*)sys_poll},
    {false, 270, (void*)sys_pselect6},
    {false, 11, (void*)sys_munmap},
    {false, 118, (void*)sys_getresuid}, // getresuid
    {false, 120, (void*)sys_getresgid}, // getresgid
    {false, 8, (void*)sys_seek},
    {true, 435, (void*)sys_clone3},
    {false, 20, (void*)sys_writev},
    {false, 61, (void*)sys_wait4},
    {false, 228, (void*)sys_clock_gettime},
    {false, 202, (void*)sys_futex},
    {false, 231, (void*)sys_exit_group},
    {false, 28, (void*)sys_enosys_stub},
    {false, 230, (void*)sys_nanosleep},
    {false, 60, (void*)sys_exit},
    {false, 59, (void*)sys_execve},
    {false, 217, (void*)sys_getdents64},
    {false, 122, (void*)sys_stub}, // setfsuid im not implementing this because uhhhhh
    {false, 123, (void*)sys_stub}, // setfsgid 
    {false, 137, (void*)sys_statfs},
    {false, 332, (void*)sys_statx},
    {false, 80, (void*)sys_chdir},
    {false, 83, (void*)sys_mkdir},
    {false, 95, (void*)sys_umask},
    {false, 81, (void*)sys_fchdir},
    {false, 74, (void*)sys_stub},
    {false, 87, (void*)sys_unlink_path}, // unlink todo
    {false, 157, (void*)sys_enosys_stub}, //todo prctl
    {false, 258, (void*)sys_mkdirat},
    {false, 165, (void*)sys_mount}, // todo mount
    {false, 162, (void*)sys_stub},
    {false, 436, (void*)sys_close_range},
    {false, 131, (void*)sys_sigaltstack},
    {false, 99, (void*)sys_sysinfo},
    {false, 221, (void*)sys_stub}, // no fadvise
    {false, 2, (void*)sys_open},
    {false, 186, (void*)sys_gettid},
    {false, 263, (void*)sys_unlink},
    {false, 247, (void*)sys_waitid}, // todo waitid
    {false, 98, (void*)sys_enosys_stub},
    {false, 90, (void*)sys_stub}
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