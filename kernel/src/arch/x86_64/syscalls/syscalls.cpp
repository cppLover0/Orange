
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <generic/locks/spinlock.hpp>

#include <generic/vfs/fd.hpp>

#include <arch/x86_64/scheduling.hpp>
#include <arch/x86_64/cpu/data.hpp>

#include <arch/x86_64/syscalls/signal.hpp>

#include <etc/errno.hpp>

extern "C" syscall_ret_t sys_hello(int a, int b ,int c) {
    DEBUG(1,"hello");
    return {0,0,0};
}

void signal_ret_sigmask(sigset_t* sigmask) {
    // it can be only called from syscall
    cpudata_t* cpdata = arch::x86_64::cpu::data();

    if(!cpdata)
        return;

    arch::x86_64::process_t* proc = cpdata->temp.proc;
    if(!proc)
        return;

    if(proc->sig->is_not_empty_sigset(sigmask) && 1) {

        memcpy(&proc->temp_sigset,&proc->current_sigset,sizeof(sigset_t));
        memcpy(&proc->current_sigset,sigmask,sizeof(sigset_t));
        proc->is_restore_sigset = 1;

        proc->ctx = *proc->sys_ctx;
        proc->ctx.rax = -EINTR;
        schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
        __builtin_unreachable();

    }
    
    return;

}

void signal_ret() {
    // it can be only called from syscall
    cpudata_t* cpdata = arch::x86_64::cpu::data();

    if(!cpdata)
        return;

    arch::x86_64::process_t* proc = cpdata->temp.proc;
    if(!proc)
        return;

    PREPARE_SIGNAL(proc) {



        proc->ctx = *proc->sys_ctx;
        proc->ctx.rax = -EINTR;
        schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
        __builtin_unreachable();

    }
    
    return;

}

long long sys_stub() {
    return -ENOSYS;
}

long long sys_zero_stub() {
    return 0;
}

arch::x86_64::syscall_item_t sys_table[] = {
    {12,(void*)sys_stub}, // brk
    {21,(void*)sys_access},
    {257,(void*)sys_openat},
    {5,(void*)sys_fstat},
    {3,(void*)sys_close},
    {0,(void*)sys_read},
    {20,(void*)sys_writev},
    {231,(void*)sys_exit_group},
    {262,(void*)sys_newfstatat},
    {9,(void*)sys_mmap},
    {17,(void*)sys_pread},
    {158,(void*)sys_arch_prctl},
    {202,(void*)sys_futex},
    {218,(void*)sys_set_tid_address},
    {273,(void*)sys_stub},
    {334,(void*)sys_stub},
    {10,(void*)sys_mprotect},
    {302,(void*)sys_prlimit64},
    {318,(void*)sys_getrandom},
    {228,(void*)sys_clock_gettime},
    {16,(void*)sys_ioctl},
    {11,(void*)sys_free},
    {8,(void*)sys_lseek},
    {33,(void*)sys_dup2},
    {13,(void*)sys_sigaction},
    {14,(void*)sys_sigprocmask},
    {28,(void*)sys_zero_stub},
    {435,(void*)sys_clone3},
    {203,(void*)sys_zero_stub},
    {1,(void*)sys_write},
    {59,(void*)sys_exec},
    {18,(void*)sys_pwrite},
    {27,(void*)sys_stub},
    {7,(void*)sys_poll},
    {72,(void*)sys_fcntl},
    {102,(void*)sys_getuid},
    {104,(void*)sys_zero_stub}, // get_gid
    {107,(void*)sys_getuid}, // get_euid,
    {108,(void*)sys_zero_stub}, // get_egid
    {96,(void*)sys_gettimeofday},
    {63,(void*)sys_uname},
    {79,(void*)sys_getcwd},
    {39,(void*)sys_getpid},
    {110,(void*)sys_getppid},
    {62,(void*)sys_kill},
    {34,(void*)sys_pause},
    {105,(void*)sys_setuid},
    {60,(void*)sys_exit},
    {111,(void*)sys_getpgrp}, 
    {56,(void*)sys_clone},
    {61,(void*)sys_wait4},
    {41,(void*)sys_socket},
    {42,(void*)sys_connect},
    {50,(void*)sys_listen},
    {44,(void*)sys_sendto},
    {32,(void*)sys_dup2},
    {109,(void*)sys_setpgid}, // set pgid todo
    {201,(void*)sys_time},
    {217,(void*)sys_getdents64},
    {80,(void*)sys_chdir},
    {81,(void*)sys_fchdir},
    {332,(void*)sys_statx},
    {267,(void*)sys_readlinkat},
    {89,(void*)sys_readlink},
    {258,(void*)sys_mkdirat},
    {83,(void*)sys_mkdir},
    {293,(void*)sys_pipe2},
    {86,(void*)sys_link},
    {265,(void*)sys_linkat},
    {22,(void*)sys_pipe},
    {88,(void*)sys_symlink},
    {266,(void*)sys_symlinkat},
    {82,(void*)sys_rename},
    {264,(void*)sys_renameat},
    {316,(void*)sys_renameat},
    {221,(void*)sys_stub},
    {90,(void*)sys_chmod},
    {91,(void*)sys_fchmod},
    {268,(void*)sys_fchmodat},
    {452,(void*)sys_fchmodat2},
    {326,(void*)sys_stub},
    {285,(void*)sys_stub},
    {77,(void*)sys_zero_stub}, // todo implement ftrunc
    {95,(void*)sys_umask},
    {230,(void*)sys_nanosleep},
    {87,(void*)sys_stub}, // priority todo implement unlink
    {55,(void*)sys_getsockopt},
    {49,(void*)sys_bind},
    {229,(void*)sys_stub}, //clock_getres
    {48,(void*)sys_shutdown},
    {106,(void*)sys_zero_stub}, // set_gid
    {157,(void*)sys_stub}, // prctl 
    {141,(void*)sys_setpriority},
    {140,(void*)sys_getpriority},
    {98,(void*)sys_stub}, // getrusage
    {37,(void*)sys_alarm},
    {38,(void*)sys_setitimer},
    {36,(void*)sys_getitimer},
    {130,(void*)sys_sigsuspend},
    {52,(void*)sys_stub}, // get peername
    {51,(void*)sys_getsockname},
    {47,(void*)sys_msg_recv},
    {43,(void*)sys_accept},
    {186,(void*)sys_getpid},
    {234,(void*)sys_tgkill}, // tgkill
    {99,(void*)sys_sysinfo},
    {439,(void*)sys_faccessat},
    {334,(void*)sys_faccessat},
    {118,(void*)sys_getresuid},
    {120,(void*)sys_zero_stub}, // getgid
    {122,(void*)sys_zero_stub}, // setfsuid
    {123,(void*)sys_zero_stub}, // setfsgid
    {121,(void*)sys_getpgid},
    {270,(void*)sys_pselect6},
    {137,(void*)sys_statfs},
    {138,(void*)sys_fstatfs},
    {74,(void*)sys_zero_stub}, // fsync
    {144,(void*)sys_zero_stub} // setscheduler
};

arch::x86_64::syscall_item_t* __syscall_find(int rax) {
    for(int i = 0; i < sizeof(sys_table) / sizeof(arch::x86_64::syscall_item_t);i++) {
        if(sys_table[i].syscall_num == rax)
            return &sys_table[i];
    }
    Log::SerialDisplay(LEVEL_MESSAGE_WARN,"unknown syscall %d",rax);
    return 0; // without it there will be ub
}

extern "C" void syscall_handler_c(int_frame_t* ctx) {

    arch::x86_64::syscall_item_t* item = __syscall_find(ctx->rax);

    if(!item) {
        assert(0,"unimplemented syscall %d !\n",ctx->rax);
        ctx->rax = -ENOSYS;
        return;
    } 

    

    cpudata_t* cpdata = arch::x86_64::cpu::data();
    arch::x86_64::process_t* proc = cpdata->temp.proc;
    proc->old_stack = ctx->rsp;

    if(1)
        proc->sys = ctx->rax;

    extern int last_sys;
    last_sys = ctx->rax;

    DEBUG(0,"sys %d from %d",ctx->rax,proc->id);

    //DEBUG(1,"sys %d from %d rip 0x%p, min_free_fd %d",ctx->rax,proc->id,ctx->rip,vfs::fdmanager::min_free(proc));
        
    proc->sys_ctx = ctx;

    long long (*sys)(std::uint64_t D, std::uint64_t S, std::uint64_t d, int_frame_t* frame) = (long long (*)(std::uint64_t, std::uint64_t, std::uint64_t, int_frame_t*))item->syscall_func;
    long long ret = sys(ctx->rdi,ctx->rsi,ctx->rdx,ctx);
    ctx->rax = ret;

    if(ret < 0)
        DEBUG(proc->is_debug,"sys %d failed with code %lli",item->syscall_num,ctx->rax);

    //DEBUG(1,"sys %d ret %d",item->syscall_num,ctx->rax);

    cpdata->temp.temp_ctx = 0;

    proc->sys_ctx = 0;

    return;
} 

void arch::x86_64::syscall::init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,(1 << 9)); // syscalls will enable interrupts when gs is swapped + stack is saved
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}

