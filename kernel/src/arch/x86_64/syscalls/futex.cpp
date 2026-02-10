
#include <cstdint>
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <arch/x86_64/syscalls/signal.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>
#include <generic/vfs/vfs.hpp>
#include <drivers/tsc.hpp>

#include <generic/locks/spinlock.hpp>

locks::spinlock futex_lock2;

long long sys_futex(int* uaddr, int op, uint32_t val, int_frame_t* ctx) {
    int operation = op & FUTEX_CMD_MASK;
    int flags = op & (FUTEX_CLOCK_REALTIME | FUTEX_PRIVATE_FLAG);

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 

    struct timespec* ts = (struct timespec*)ctx->r10;

    DEBUG(proc->is_debug,"futex op %d, val %d, uaddr 0x%p, flags %d, raw_op %d",operation,val,uaddr,flags,op);

    switch(operation) {
    case FUTEX_WAIT: {
        
        SYSCALL_IS_SAFEZ(uaddr,4096);

        int v = 0;
        v = ((std::atomic<int>*)uaddr)->load();

        if(v != val)
            return -EAGAIN;

        std::uint64_t t = 0;
        if(ts) {
            t = (ts->tv_sec * (1000 * 1000)) + (ts->tv_nsec / 1000);
            t += drivers::tsc::currentus();
        }

        DEBUG(proc->is_debug,"Waiting for futex, pointer: 0x%p excepted: %d, pointer_value %d in proc %d, ts->tv_nsec %lli ts->tv_sec %lli",uaddr,val,v,proc->id,ts != nullptr ? ts->tv_nsec : 0, ts != nullptr ? ts->tv_sec : 0);
            
        arch::x86_64::scheduling::futexwait(proc,&v,val,uaddr,t);

        futex_lock2.unlock();
        yield();
            
        while(proc->futex_lock.test()) {
            if(ts) {
                 if(proc->ts < drivers::tsc::currentus()) {
                     proc->futex_lock.unlock(); 
                      return -ETIMEDOUT;
                  }
                }

              PREPARE_SIGNAL(proc) {
                  proc->futex_lock.unlock();
                   signal_ret();
               }

               yield();
        }
            
        return 0;
    };
    case FUTEX_WAKE: {
        int c = arch::x86_64::scheduling::futexwake(proc,uaddr,val);
        return c;
    }
    default:
        Log::SerialDisplay(LEVEL_MESSAGE_FAIL,"unsupported futex op %d\n",op);
        return 0;
    };
    return 0;
}

// syscall_ret_t sys_futex_wait(int* pointer, int excepted, struct timespec* ts) {

//     futex_lock2.lock();
//     int v = 0;
//     v = ((std::atomic<int>*)pointer)->load();

//     if(v != excepted) {
//         futex_lock2.unlock();
//         return {0,EAGAIN,0};
//     }

//     arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 

//     std::uint64_t t = 0;
//     if(ts) {
//         t = (ts->tv_sec * (1000 * 1000)) + (ts->tv_nsec / 1000);
//         t += drivers::tsc::currentus();
//     }

    

//     int copied_pointer_val = v;
//     DEBUG(0,"Waiting for futex, pointer: 0x%p excepted: %d, pointer_value %d in proc %d, ts->tv_nsec %lli ts->tv_sec %lli",pointer,excepted,copied_pointer_val,proc->id,ts != nullptr ? ts->tv_nsec : 0, ts != nullptr ? ts->tv_sec : 0);
    
//     arch::x86_64::scheduling::futexwait(proc,&copied_pointer_val,excepted,pointer,t);

//     futex_lock2.unlock();
//     yield();
    
//     while(proc->futex_lock.test()) {
//         if(ts) {
//             if(proc->ts < drivers::tsc::currentus()) {
//                 proc->futex_lock.unlock(); 
//                 return {0,ETIMEDOUT,0};
//             }
//         }

//         PREPARE_SIGNAL(proc) {
//             proc->futex_lock.unlock();
//             signal_ret();
//         }

//         yield();
//     }

//     return {0,0,0};
// }

// syscall_ret_t sys_futex_wake(int* pointer) {
//     futex_lock2.lock();
//     arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc; 
//     DEBUG(0,"Wakeup futex with pointer 0x%p in proc %d",pointer,proc->id);
//     int c = arch::x86_64::scheduling::futexwake(proc,pointer);
//     futex_lock2.unlock();
//     return {0,0,0};
// }
