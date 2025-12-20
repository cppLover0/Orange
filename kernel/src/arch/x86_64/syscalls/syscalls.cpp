
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <generic/locks/spinlock.hpp>

#include <arch/x86_64/cpu/data.hpp>

#include <etc/errno.hpp>

arch::x86_64::syscall_item_t sys_table[] = {
    {1,(void*)sys_futex_wake},
    {2,(void*)sys_futex_wait},
    {3,(void*)sys_openat},
    {4,(void*)sys_read},
    {5,(void*)sys_write},
    {6,(void*)sys_seek},
    {7,(void*)sys_close},
    {8,(void*)sys_tcb_set},
    {9,(void*)sys_libc_log},
    {10,(void*)sys_exit},
    {11,(void*)sys_mmap},
    {12,(void*)sys_free},
    {13,(void*)sys_stat},
    {14,(void*)sys_pipe},
    {15,(void*)sys_fork},
    {16,(void*)sys_dup},
    {17,(void*)sys_dup2},
    {18,(void*)sys_create_dev},
    {19,(void*)sys_iopl},
    {20,(void*)sys_ioctl},
    {21,(void*)sys_create_ioctl},
    {22,(void*)sys_setup_tty},
    {23,(void*)sys_isatty},
    {24,(void*)sys_setupmmap},
    {25,(void*)sys_access_framebuffer},
    {26,(void*)sys_ptsname},
    {27,(void*)sys_setup_ring_bytelen},
    {28,(void*)sys_read_dir},
    {29,(void*)sys_exec},
    {30,(void*)sys_getpid},
    {31,(void*)sys_getppid},
    {32,(void*)sys_gethostname},
    {33,(void*)sys_getcwd},
    {34,(void*)sys_waitpid},
    {35,(void*)sys_fcntl},
    {36,(void*)sys_fchdir},
    {37,(void*)sys_sleep},
    {38,(void*)sys_alloc_dma},
    {39,(void*)sys_map_phys},
    {40,(void*)sys_free_dma},
    {41,(void*)sys_connect},
    {42,(void*)sys_accept},
    {43,(void*)sys_bind},
    {44,(void*)sys_socket},
    {45,(void*)sys_listen},
    {46,(void*)sys_timestamp},
    {47,(void*)sys_mkfifoat},
    {48,(void*)sys_poll},
    {49,(void*)sys_readlinkat},
    {50,(void*)sys_link},
    {51,(void*)sys_mkdirat},
    {52,(void*)sys_chmod},
    {53,(void*)sys_enabledebugmode},
    {54,(void*)sys_clone},
    {56,(void*)sys_ttyname},
    {57,(void*)sys_breakpoint},
    {58,(void*)sys_copymemory},
    {59,(void*)sys_setpriority},
    {60,(void*)sys_getpriority},
    {61,(void*)sys_yield},
    {62,(void*)sys_rename},
    {63,(void*)sys_printdebuginfo},
    {64,(void*)sys_dmesg},
    {65,(void*)sys_enabledebugmodepid}
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
        return;
    } else if(!item->syscall_func) {
        return;
    } 

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;
    proc->sys = ctx->rax;

    extern int last_sys;
    last_sys = ctx->rax;

    arch::x86_64::cpu::data()->last_sys = ctx->rax;

    DEBUG(0,"sys %d from %d",ctx->rax,proc->id);
        

    syscall_ret_t (*sys)(std::uint64_t D, std::uint64_t S, std::uint64_t d, int_frame_t* frame) = (syscall_ret_t (*)(std::uint64_t, std::uint64_t, std::uint64_t, int_frame_t*))item->syscall_func;
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

