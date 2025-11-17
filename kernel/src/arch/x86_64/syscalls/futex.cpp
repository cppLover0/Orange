
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

syscall_ret_t sys_futex_wait(int* pointer, int excepted) {

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 
    int copied_pointer_val = 0;
    copy_in_userspace(proc,&copied_pointer_val,pointer,sizeof(int));
    //DEBUG(proc->is_debug,"Waiting for futex, pointer: 0x%p excepted: %d, pointer_value %d in proc %d",pointer,excepted,copied_pointer_val,proc->id);
    arch::x86_64::scheduling::futexwait(proc,&copied_pointer_val,excepted,pointer);
    yield();
    return {0,0,0};
}

syscall_ret_t sys_futex_wake(int* pointer) {
    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 
    //DEBUG(proc->is_debug,"Wakeup futex with pointer 0x%p in proc %d",pointer,proc->id);
    arch::x86_64::scheduling::futexwake(proc,pointer);
    return {0,0,0};
}
