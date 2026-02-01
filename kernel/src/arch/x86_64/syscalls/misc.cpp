

#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/vmm.hpp>

#include <generic/vfs/fd.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <drivers/cmos.hpp>

#include <drivers/tsc.hpp>

#include <etc/errno.hpp>

#include <drivers/hpet.hpp>

#include <drivers/kvmtimer.hpp>

#include <generic/time.hpp>

#include <generic/vfs/vfs.hpp>

#include <etc/bootloaderinfo.hpp>

std::atomic<std::uint64_t> zznext = 0;
std::uint64_t __zzrand() {
    uint64_t t = __rdtsc();
    return t * (++zznext);
}

long long sys_getrandom(char *buf, size_t count, unsigned int flags) {
    if(!buf)
        return -EINVAL;
    for(std::uint64_t i = 0;i < count;i++) {
        buf[i] = __zzrand() & 0xFF;
    }
    return 0;
}

# define CLOCK_REALTIME			0
/* Monotonic system-wide clock.  */
# define CLOCK_MONOTONIC		1
/* High-resolution timer from the CPU.  */
# define CLOCK_PROCESS_CPUTIME_ID	2
/* Thread-specific CPU-time clock.  */
# define CLOCK_THREAD_CPUTIME_ID	3
/* Monotonic system-wide clock, not adjusted for frequency scaling.  */
# define CLOCK_MONOTONIC_RAW		4
/* Identifier for system-wide realtime clock, updated only on ticks.  */
# define CLOCK_REALTIME_COARSE		5
/* Monotonic system-wide clock, updated only on ticks.  */
# define CLOCK_MONOTONIC_COARSE		6
/* Monotonic system-wide clock that includes time spent in suspension.  */
# define CLOCK_BOOTTIME			7
/* Like CLOCK_REALTIME but also wakes suspended system.  */
# define CLOCK_REALTIME_ALARM		8
/* Like CLOCK_BOOTTIME but also wakes suspended system.  */
# define CLOCK_BOOTTIME_ALARM		9
/* Like CLOCK_REALTIME but in International Atomic Time.  */
# define CLOCK_TAI			11

std::atomic<std::uint64_t> clk_mnt_sync = 0;

long long sys_clock_gettime(clockid_t which_clock, struct __kernel_timespec *tp) {
    switch(which_clock) {
    case CLOCK_TAI:
    case CLOCK_REALTIME_COARSE:
    case CLOCK_REALTIME:
        tp->tv_sec = getUnixTime();
        tp->tv_nsec = 0;
        return 0;
    case CLOCK_THREAD_CPUTIME_ID:
    case CLOCK_PROCESS_CPUTIME_ID:
    case CLOCK_BOOTTIME:
    case CLOCK_MONOTONIC_COARSE:
    case CLOCK_MONOTONIC_RAW:
    case CLOCK_MONOTONIC:
        std::uint64_t ts = drivers::tsc::currentnano();
        tp->tv_sec = ts / 1000000000;
        tp->tv_nsec = ts % 1000000000;
        return 0;
    };
    return -EINVAL;
}

long long sys_gettimeofday(timeval* tv, void* tz) { // tz unused
    std::uint64_t ts = drivers::tsc::currentnano();
    if(tv) {
        tv->tv_sec = getUnixTime();
        tv->tv_usec = (ts % 1000000000LL) / 1000LL;
    }
    return 0;
}

long long sys_uname(old_utsname* uname) {

    if(!uname)
        return -EINVAL;

    memset(uname,0,sizeof(old_utsname));

    memcpy(uname->sysname,"Linux",sizeof("Linux"));
    memcpy(uname->nodename,"orange-pc",sizeof("orange-pc"));
    memcpy(uname->machine,"x86_64",sizeof("x86_64"));

    return 0;

}

long long sys_time(std::uint64_t* t) {
    SYSCALL_IS_SAFEZ(t,4096);
    std::uint64_t time = getUnixTime();
    if(t)   
        *t = time;
    return time;
}

long long sys_nanosleep(int clock, int flags, timespec* rqtp, int_frame_t* ctx) {
    timespec* rmtp = (timespec*)ctx->r10;
    std::uint64_t us = (rqtp->tv_nsec / 1000) + (rqtp->tv_sec * 1000000);
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(1) { // tsc sleep
        std::uint64_t current = drivers::tsc::currentnano();
        std::uint64_t end = us * 1000;
        while((drivers::tsc::currentnano() - current) < end) {
            if((drivers::tsc::currentnano() - current) < 15000) {
                asm volatile("pause");
            } else
                yield();
            PREPARE_SIGNAL(proc) {
                std::uint64_t ns = us * 1000;
                rmtp->tv_sec = ns / 1000000000;
                rmtp->tv_nsec = ns % 1000000000;
                signal_ret();
            }
        }
    } 
    return 0;
} 

long long sys_sysinfo(sysinfo* info) {
    SYSCALL_IS_SAFEZ(info,4096);

    if(!info)
        return -EINVAL;

    info->uptime = drivers::tsc::currentus() / (1000 * 1000);
    
    return 0;
}