
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

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
    {13,(void*)sys_stat}
};

arch::x86_64::syscall_item_t* __syscall_find(int rax) {
    for(int i = 0; i < sizeof(sys_table) / sizeof(arch::x86_64::syscall_item_t);i++) {
        if(sys_table[i].syscall_num == rax)
            return &sys_table[i];
    }
}

extern "C" void syscall_handler_c(int_frame_t* ctx) {
    memory::paging::enablekernel();
    arch::x86_64::syscall_item_t* item = __syscall_find(ctx->rax);
    syscall_ret_t (*sys)(std::uint64_t D, std::uint64_t S, std::uint64_t d, int_frame_t* frame) = (syscall_ret_t (*)(std::uint64_t, std::uint64_t, std::uint64_t, int_frame_t*))item->syscall_func;
    syscall_ret_t ret = sys(ctx->rdi,ctx->rsi,ctx->rdx,ctx);
    if(ret.is_rdx_ret) {
        ctx->rdx = ret.ret_val;
    }
   

    ctx->rax = ret.ret;
    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"sys %d\n",ctx->rax);
    return;
} 

void arch::x86_64::syscall::init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,0); // syscalls will support interrupts
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}