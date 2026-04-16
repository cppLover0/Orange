#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <utils/linux.hpp>
#include <generic/userspace/robust.hpp>
#include <generic/userspace/safety.hpp>
#include <utils/random.hpp>
#include <utils/kernel_info.hpp>
#include <generic/time.hpp>
#include <generic/userspace/futex.hpp>

locks::spinlock futex_lock2;

long long sys_futex(int* uaddr, int op, uint32_t val, timespec* ts) {
    int operation = op & FUTEX_CMD_MASK;
    int flags = op & (FUTEX_CLOCK_REALTIME | FUTEX_PRIVATE_FLAG);

    thread* proc = current_proc;

    if(!is_safe_to_rw(proc, (std::uint64_t)uaddr, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)ts, PAGE_SIZE))
        return -EFAULT;

    if(proc->is_debug) klibc::debug_printf("futex op %d, val %d, uaddr 0x%p, flags %d, raw_op %d\n",operation,val,uaddr,flags,op);

    switch(operation) {
    case FUTEX_WAIT_BITSET:
    case FUTEX_WAIT: {

        int v = 0;
        v = ((std::atomic<int>*)uaddr)->load();

        if(v != (int)val)
            return -EAGAIN;

        std::uint64_t t = 0;
        if(ts) {
            t = (ts->tv_sec * (1000 * 1000)) + (ts->tv_nsec / 1000);
            t += (time::timer->current_nano() / 1000);
        }

        if(proc->is_debug) klibc::debug_printf("Waiting for futex, pointer: 0x%p excepted: %d, pointer_value %d in proc %d, ts->tv_nsec %lli ts->tv_sec %lli\n",uaddr,val,v,proc->id,ts != nullptr ? ts->tv_nsec : 0, ts != nullptr ? ts->tv_sec : 0);
            
        process::futex_wait(proc, uaddr);

        futex_lock2.unlock();
        process::yield();
            
        while(proc->futex.load() != 0) {
            if(ts) {
                if(t < (time::timer->current_nano() / 1000)) {
                    proc->futex.store(0); 
                    return -ETIMEDOUT;
                }
            }

               process::yield();
        }
            
        return 0;
    };
    case FUTEX_WAKE: {
        int c = process::futex_wake(proc, uaddr, val);
        return c;
    }
    default:
        //assert(0, "unimplemented futex op %d", operation);
        return -ENOSYS;
    };
    return 0;
}
