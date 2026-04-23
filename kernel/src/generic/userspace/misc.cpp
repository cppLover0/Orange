#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <utils/linux.hpp>
#include <generic/userspace/robust.hpp>
#include <utils/signal_ret.hpp>
#include <generic/userspace/safety.hpp>
#include <utils/random.hpp>
#include <utils/kernel_info.hpp>
#include <generic/time.hpp>

long long sys_getrandom(char* buf, std::uint64_t count, std::uint32_t flags) {
    (void)flags;
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)buf, count)) {
        return -EFAULT;
    }

    if(!buf)
        return -EINVAL;
    
    if(current->is_debug) {
        klibc::debug_printf("trying to get random buffer 0x%p, count %lli, flags 0x%p, random bytes:", buf, count, flags);
    }

    for(std::uint64_t i = 0;i < count;i++) {
        char random_byte = random::random() & 0xFF;
        if(current->is_debug) {
            klibc::debug_printf(" %p ", random_byte);
        }
        buf[i] = random_byte;
    }

    if(current->is_debug) {
        klibc::debug_printf("\n");
    }

    return count;

}

long long sys_gettimeofday(timeval* tv, void* tz) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)tv, PAGE_SIZE)) {
        return -EFAULT;
    }

    if(!is_safe_to_rw(current, (std::uint64_t)tz, PAGE_SIZE)) {
        return -EFAULT;
    }

    assert(time::timer,"no timer :(");

    if(tv) {
        tv->tv_sec = time::current_unix_time;
        tv->tv_usec = time::timer->current_nano() / 1000;
    }

    return 0;
}

long long sys_uname(old_utsname* utsname) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)utsname, PAGE_SIZE)) {
        return -EFAULT;
    }

    if(utsname == nullptr)
        return -EINVAL;

    klibc::memset(utsname, 0, sizeof(old_utsname));
    klibc::memcpy(utsname->sysname, "Orange", sizeof("Orange"));
    klibc::memcpy(utsname->release, kernel_info::version(), klibc::strlen(kernel_info::version()) + 1);
    klibc::memcpy(utsname->machine, "x86_64", sizeof("x86_64"));

    return 0;

}

long long sys_time(std::uint64_t* time) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)time, PAGE_SIZE)) {
        return -EFAULT;
    }

    if(time == nullptr)
        return time::current_unix_time;

    *time = time::current_unix_time;

    return 0;
}

std::atomic<std::uint64_t> clk_mnt_sync = 0;

long long sys_clock_gettime(clockid_t which_clock, struct timespec *tp) {
    switch(which_clock) {
    case CLOCK_TAI:
    case CLOCK_REALTIME_COARSE:
    case CLOCK_REALTIME:
        tp->tv_sec = time::current_unix_time;
        tp->tv_nsec = 0;
        return 0;
    case CLOCK_THREAD_CPUTIME_ID:
    case CLOCK_PROCESS_CPUTIME_ID:
    case CLOCK_BOOTTIME:
    case CLOCK_MONOTONIC_COARSE:
    case CLOCK_MONOTONIC_RAW:
    case CLOCK_MONOTONIC:
        std::uint64_t ts = time::timer->current_nano();
        if(ts < clk_mnt_sync) ts = clk_mnt_sync.load();
        else clk_mnt_sync.store(ts);
        tp->tv_sec = ts / 1000000000;
        tp->tv_nsec = ts % 1000000000;
        return 0;
    };
    return -EINVAL;
}

long long sys_nanosleep(int clock, int flags, timespec* rqtp, timespec* rmtp) {
    (void)clock;
    (void)flags;
    (void)rmtp;

    assert(rqtp, ":(");

    std::uint64_t us = (rqtp->tv_nsec / 1000) + (rqtp->tv_sec * 1000000);
    thread* proc = current_proc;
    (void)proc;
    if(1) { // tsc sleep
        std::uint64_t current = time::timer->current_nano();
        std::uint64_t end = us * 1000;
        while((time::timer->current_nano() - current) < end) {
            if((time::timer->current_nano() - current) < 1500) {
                asm volatile("pause");
            } else
                process::yield();

            is_signal_ret(proc) {
                signal_ret(proc);
            }

        }
    } 
    return 0;
} 

extern std::uint32_t proc_count;
extern std::size_t free_mem;
extern std::size_t memory_size;

long long sys_sysinfo(sysinfo* out) {

    thread* current_thread = current_proc;

    if(!is_safe_to_rw(current_thread, (std::uint64_t)out, PAGE_SIZE))
        return -EFAULT;

    if(out == nullptr)
        return -EINVAL;

    sysinfo info = {};
    info.totalram = memory_size;
    info.freeram = free_mem;
    info.procs = proc_count & 0xFFFF;
    info.mem_unit = 1; // for 64 bit there's no bad things for it
    info.uptime = time::timer->current_nano() / (1000 * 1000 * 1000);
    *out = info;
    return 0;
}

#include <generic/mp.hpp>

long long sys_cpucount() {
    return mp::cpu_count();
}