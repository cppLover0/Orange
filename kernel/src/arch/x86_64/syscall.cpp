#include <cstdint>
#include <arch/x86_64/cpu/idt.hpp>
#include <arch/x86_64/syscall.hpp>
#include <arch/x86_64/assembly.hpp>
#include <utils/assert.hpp>
#include <klibc/stdio.hpp>
#include <utils/errno.hpp>
#include <generic/userspace/syscall_list.hpp>
#include <generic/time.hpp>

long long sys_stub() {
    return 0;
}

long long sys_enosys_stub() {
    return -ENOSYS;
}

extern "C" void syscall_handler();

syscall_item syscall_table[] = {
    {false, 0, (void*)sys_enosys_stub},
    {false, 1, (void*)sys_read},
    {false, 2, (void*)sys_write},
    {false, 3, (void*)sys_mmap},
    {false, 4, (void*)sys_munmap},
    {false, 5, (void*)sys_close},
    {false, 6, (void*)sys_futex},
    {true, 7, (void*)sys_exit_group},
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
    {true, 27, (void*)sys_clone3}, // used only for fork
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
    {false, 38, (void*)sys_wait4}, // unused
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
    {true, 59, (void*)sys_clone},// unused
    {true, 60, (void*)sys_newthread},
    {false, 61, (void*)sys_getpgid},
    {false, 62, (void*)sys_getgid},
    {false, 63, (void*)sys_ttyname},
    {false, 64, (void*)sys_sysinfo},
    {false, 65, (void*)sys_cpucount},
    {false, 66, (void*)sys_sigreturn},
    {false, 67, (void*)sys_kill},
    {false, 68, (void*)sys_pause},
    {false, 69, (void*)sys_listen},
    {false, 70, (void*)sys_accept},
    {false, 71, (void*)sys_socket},
    {false, 72, (void*)sys_connect},
    {false, 73, (void*)sys_bind},
    {false, 74, (void*)sys_recvfrom},
    {false, 75, (void*)sys_sendto},
    {false, 76, (void*)sys_msg_recv},
    {false, 77, (void*)sys_msg_send},
    {false, 78, (void*)sys_link},
    {false, 79, (void*)sys_linkat},
    {false, 80, (void*)sys_getsockopt},
    {false, 81, (void*)sys_setsockopt},
    {false, 82, (void*)sys_libclog},
    {false, 83, (void*)sys_ptsname},
    {false, 84, (void*)sys_enabledebug},
    {false, 85, (void*)sys_debugnum},
    {false, 86, (void*)sys_fchmod},
    {false, 87, (void*)sys_rename},
    {false, 88, (void*)sys_renameat},
    {false, 89, (void*)sys_getsockname},
    {false, 90, (void*)sys_socketpair},
    {false, 91, (void*)sys_pwrite64},
    {false, 92, (void*)sys_setsid},
    {false, 93, (void*)sys_getitimer},
    {false, 94, (void*)sys_setitimer},
    {false, 95, (void*)sys_shmat},
    {false, 96, (void*)sys_shmctl},
    {false, 97, (void*)sys_shmget},
    {false, 98, (void*)sys_shmdt},
    {false, 99, (void*)sys_getaffinity},
    {false, 100, (void*)sys_pidstat},
    {false, 101, (void*)sys_selfexe},
    {false, 102, (void*)sys_epoll_create},
    {false, 103, (void*)sys_epoll_ctl},
    {false, 104, (void*)sys_epoll_wait},
    {false, 105, (void*)sys_shutdown},
    {false, 106, (void*)sys_ftruncate},
    {false, 107, (void*)sys_fchownat},
    {false, 108, (void*)sys_fsync},
    {false, 109, (void*)sys_stackinfo},
    {false, 110, (void*)sys_fstatfs},
    {false, 111, (void*)sys_eventfd_create}
};

// long long sys_shmat(int shmid, std::uint64_t hint, int shmflg);
// long long sys_shmctl(int shmid, int cmd, struct shmid_ds *buf);
// long long sys_shmget(int key, size_t size, int shmflg);
// long long sys_shmdt(std::uint64_t base);

syscall_item* find_syscall(long long num) {
    return &syscall_table[num];
}

extern "C" void syscall_handler_c(x86_64::idt::int_frame_t* ctx) {
    syscall_item* current_sys = find_syscall(ctx->rax);
    if(current_sys == nullptr) {
        assert(0, "unimplemented syscall %lli",ctx->rax);
    }

    thread* current = current_proc;
    current->signal_ctx = *ctx; // its used to return to userspace when there's signal 

    current->last_syscall = current_sys->num;

    long long ret = 0;

#ifdef SYSCALL_PROFILING
    std::uint64_t start = time::timer->current_nano();
#endif

    //  if(current->is_debug)
    //      klibc::debug_printf("sys %d rdi 0x%p rsi 0x%p rdx 0x%p cwd %s\n", current_sys->num, ctx->rdi, ctx->rsi, ctx->rdx, current->cwd);

    if(current_sys->is_ctx_passed) {
        auto func = (long long (*)(x86_64::idt::int_frame_t*, long long, long long, long long, long long, long long, long long))(current_sys->sys);
        ret = func(ctx, ctx->rdi, ctx->rsi, ctx->rdx, ctx->r10, ctx->r8, ctx->r9);     
    } else {
        auto func = (long long (*)(long long, long long, long long, long long, long long, long long))(current_sys->sys);
        ret = func(ctx->rdi, ctx->rsi, ctx->rdx, ctx->r10, ctx->r8, ctx->r9);     
    }

#ifdef SYSCALL_PROFILING
    std::uint64_t end = time::timer->current_nano();
    if(current_sys->num != 6 && current_sys->num != 21 && current_sys->num != 76 && current_sys->num != 72 && current_sys->num != 74) {
        klibc::serial_printf("sys %d took %lli us, %lli ms\n", current_sys->num, (end - start) / 1000, ((end - start) / 1000) / 1000);
    }
#endif

    if(current->is_debug && current_sys->num != 6 && current_sys->num != 5)
         klibc::debug_printf("sys %d ret %lli\n", current_sys->num, ret);

    assert(ctx->cr3 != 0, "uh nuh ");

    ctx->rax = ret;

    if(current->is_restore_sigset) {
        klibc::memcpy(&current->sigset,&current->temp_sigset,sizeof(sigset_t));
        current->is_restore_sigset = 0;
    }

    return;
}

void syscall::init() {
    assembly::wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    assembly::wrmsr(LSTAR,(uint64_t)syscall_handler);
    assembly::wrmsr(STAR_MASK,(1 << 9)); // syscalls will enable interrupts when gs is swapped + stack is saved
    assembly::wrmsr(EFER,assembly::rdmsr(EFER) | 1);
}