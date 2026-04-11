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
        return -EINVAL;

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
        tp->tv_sec = ts / 1000000000;
        tp->tv_nsec = ts % 1000000000;
        return 0;
    };
    return -EINVAL;
}
