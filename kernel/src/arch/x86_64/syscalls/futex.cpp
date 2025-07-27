
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

syscall_ret_t sys_futex_wait(int* pointer, int excepted) {
    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 
    int copied_pointer_val = 0;
    copy_in_userspace(proc,&copied_pointer_val,pointer,sizeof(int));
    arch::x86_64::scheduling::futexwait(proc,&copied_pointer_val,excepted);
    return {0,0,0};
}

syscall_ret_t sys_futex_wake(int* pointer) {
    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 
    arch::x86_64::scheduling::futexwake(proc,pointer);
    return {0,0,0};
}
