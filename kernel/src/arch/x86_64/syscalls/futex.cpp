
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <arch/x86_64/syscalls/signal.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>
#include <generic/vfs/vfs.hpp>
#include <drivers/tsc.hpp>

syscall_ret_t sys_futex_wait(int* pointer, int excepted, struct timespec* ts) {

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 

    std::uint64_t t = 0;
    if(ts) {
        t = (ts->tv_sec * (1000 * 1000)) + (ts->tv_nsec / 1000);
        t += drivers::tsc::currentus();
    }

    if(*pointer != excepted)
        return {0,EAGAIN,0};

    int copied_pointer_val = 0;
    copy_in_userspace(proc,&copied_pointer_val,pointer,sizeof(int));
    DEBUG(proc->is_debug,"Waiting for futex, pointer: 0x%p excepted: %d, pointer_value %d in proc %d, ts->tv_nsec %lli ts->tv_sec %lli",pointer,excepted,copied_pointer_val,proc->id,ts != nullptr ? ts->tv_nsec : 0, ts != nullptr ? ts->tv_sec : 0);
    
    arch::x86_64::scheduling::futexwait(proc,&copied_pointer_val,excepted,pointer,t);
    yield();
    
    while(proc->futex_lock.test()) {
        if(ts) {
            if(proc->ts < drivers::tsc::currentus()) {
                proc->futex_lock.unlock(); 
                return {0,ETIMEDOUT,0};
            }
        }

        PREPARE_SIGNAL(proc) {
            proc->futex_lock.unlock();
            signal_ret();
        }

        yield();
    }

    return {0,0,0};
}

syscall_ret_t sys_futex_wake(int* pointer) {
    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 
    DEBUG(proc->is_debug,"Wakeup futex with pointer 0x%p in proc %d",pointer,proc->id);
    int c = arch::x86_64::scheduling::futexwake(proc,pointer);
    return {0,0,0};
}
