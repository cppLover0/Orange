
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <generic/locks/spinlock.hpp>

#include <arch/x86_64/cpu/data.hpp>

#include <etc/errno.hpp>

syscall_ret_t sys_hello() {
    DEBUG(1,"hello");
    return {0,0,0};
}

#define MAX_SYSCALL 100

void* sys_table0[] = {
    (void*)sys_hello, // 0
    (void*)sys_futex_wake, // 1
    (void*)sys_futex_wait, // 2
    (void*)sys_openat, // 3
    (void*)sys_read, // 4
    (void*)sys_write, // 5
    (void*)sys_seek, // 6
    (void*)sys_close, // 7
    (void*)sys_tcb_set, // 8
    (void*)sys_libc_log, // 9
    (void*)sys_exit, // 10
    (void*)sys_mmap, // 11
    (void*)sys_free, // 12
    (void*)sys_stat, // 13
    (void*)sys_pipe, // 14
    (void*)sys_fork, // 15
    (void*)sys_dup, // 16
    (void*)sys_dup2, // 17
    (void*)sys_create_dev, // 18
    (void*)sys_iopl, // 19 
    (void*)sys_ioctl, // 20 
    (void*)sys_create_ioctl, // 21
    (void*)sys_setup_tty, // 22
    (void*)sys_isatty, // 23
    (void*)sys_setupmmap, // 24
    (void*)sys_access_framebuffer, // 25
    (void*)sys_ptsname, // 26
    (void*)sys_setup_ring_bytelen, // 27
    (void*)sys_read_dir, // 28
    (void*)sys_exec, // 29
    (void*)sys_getpid, // 30
    (void*)sys_getppid, // 31
    (void*)sys_gethostname, // 32
    (void*)sys_getcwd, // 33
    (void*)sys_waitpid, // 34
    (void*)sys_fcntl, // 35
    (void*)sys_fchdir, // 36
    (void*)sys_sleep, // 37
    (void*)sys_alloc_dma, // 38
    (void*)sys_map_phys, // 39
    (void*)sys_free_dma, // 40 
    (void*)sys_connect, // 41 
    (void*)sys_accept, // 42
    (void*)sys_bind, // 43
    (void*)sys_socket, // 44
    (void*)sys_listen, // 45
    (void*)sys_timestamp, // 46 
    (void*)sys_mkfifoat, // 47
    (void*)sys_poll, // 48
    (void*)sys_readlinkat, // 49
    (void*)sys_link, // 50
    (void*)sys_mkdirat, // 51
    (void*)sys_chmod, // 52
    (void*)sys_enabledebugmode, // 53
    (void*)sys_clone, // 54
    (void*)sys_hello, // 55
    (void*)sys_ttyname, // 55 + 1
    (void*)sys_breakpoint, // 56 + 1
    (void*)sys_copymemory, // 57 + 1
    (void*)sys_setpriority, // 58 + 1
    (void*)sys_getpriority, // 59 + 1
    (void*)sys_yield, // 60 + 1
    (void*)sys_rename, // 61 + 1
    (void*)sys_printdebuginfo, // 62 + 1
    (void*)sys_dmesg, // 63 + 1
    (void*)sys_enabledebugmodepid, // 64 + 1
    (void*)sys_socketpair, // 65 + 1
    (void*)sys_getuid, // 66 + 1
    (void*)sys_setuid, // 67 + 1
    (void*)sys_getsockname, // 68  + 1
    (void*)sys_getsockopt, // 69 + 1
    (void*)sys_msg_send, // 70 + 1
    (void*)sys_eventfd_create, // 71 + 1
    (void*)sys_msg_recv, // 72 + 1
    (void*)sys_kill, // 73 + 1 
    (void*)sys_shutdown, // 74 + 1
    (void*)sys_shmget, // 76
    (void*)sys_shmat, // 77
    (void*)sys_shmdt, // 78
    (void*)sys_shmctl // 79
};

void* __syscall_find(int rax) {
    if(rax < MAX_SYSCALL && rax > 0)
        return sys_table0[rax];
    Log::SerialDisplay(LEVEL_MESSAGE_WARN,"unknown syscall %d",rax);
    return 0; // without it there will be ub
}

extern "C" void syscall_handler_c(int_frame_t* ctx) {

    void* item = __syscall_find(ctx->rax);

    if(!item) {
        return;
    } 

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;
    proc->sys = ctx->rax;

    extern int last_sys;
    last_sys = ctx->rax;

    arch::x86_64::cpu::data()->last_sys = ctx->rax;

    DEBUG(0,"sys %d from %d rip 0x%p",ctx->rax,proc->id,ctx->rip);
        

    syscall_ret_t (*sys)(std::uint64_t D, std::uint64_t S, std::uint64_t d, int_frame_t* frame) = (syscall_ret_t (*)(std::uint64_t, std::uint64_t, std::uint64_t, int_frame_t*))item;
    syscall_ret_t ret = sys(ctx->rdi,ctx->rsi,ctx->rdx,ctx);
    if(ret.is_rdx_ret) {
        ctx->rdx = ret.ret_val;
        if((std::int64_t)ret.ret_val < 0)
            DEBUG(proc->is_debug,"rdx ret 0x%p smaller than zero from sys %d",ret.ret_val,ctx->rax);
    }

    last_sys = 0;
    DEBUG(0,"sys ret %d from proc %d ",ret.ret,proc->id);

    if(ret.ret != 0 && ret.ret != EAGAIN)
        DEBUG(proc->is_debug,"Syscall %d from proc %d fails with code %d",ctx->rax,proc->id,ret.ret);

    ctx->rax = ret.ret; 
    return;
} 

void arch::x86_64::syscall::init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,(1 << 9)); // syscalls will enable interrupts when gs is swapped + stack is saved
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}

