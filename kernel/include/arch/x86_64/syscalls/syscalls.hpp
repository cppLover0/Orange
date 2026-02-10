
#include <cstdint>

#pragma once

#include <generic/mm/paging.hpp>

#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/scheduling.hpp>
#include <etc/bootloaderinfo.hpp>
#include <etc/libc.hpp>

#include <generic/mm/vmm.hpp>
#include <etc/etc.hpp>

#include <arch/x86_64/cpu/data.hpp>

inline void copy_in_userspace(arch::x86_64::process_t* proc,void* dest, void* src, std::uint64_t size) {

    void* ddest = 0;
    void* ssrc = 0;

    ddest = dest;
    ssrc = src;

    memcpy(ddest,ssrc,size);
}

inline void copy_in_userspace_string(arch::x86_64::process_t* proc,void* dest, void* src, std::uint64_t size) {

    void* ddest = 0;
    void* ssrc = 0;

    ddest = dest;
    ssrc = src;

    memcpy(ddest,ssrc,strlen((char*)ssrc) > size ? size : strlen((char*)ssrc));
}

inline void zero_in_userspace(arch::x86_64::process_t* proc,void* buf, std::uint64_t size) {
    
    void* bbuf;

    bbuf = buf;

    memset(bbuf,0,size);
}

class syscall_safe {
public:

    static inline int is_safe(std::uint64_t ptr) {
        if(ptr >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

    static inline int is_safe(void* ptr) {
        if((std::uint64_t)ptr >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

    static inline int is_safe(std::uint64_t ptr,std::uint64_t size) {
        if(ptr + size >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

    static inline int is_safe(void* ptr,std::uint64_t size) {
        if((std::uint64_t)ptr + size >= BootloaderInfo::AccessHHDM())
            return 0;
        return 1;
    }

};

#define SYSCALL_IS_SAFEA(x,sz) \
    if(!syscall_safe::is_safe(x,sz)) \
        return {0,EFAULT,0};

#define SYSCALL_IS_SAFEZ(x,sz) \
    if(!syscall_safe::is_safe(x,sz)) \
        return -EFAULT;

#define SYSCALL_IS_SAFEB(x,sz) \
    if(!syscall_safe::is_safe(x,sz)) \
        return {0,EFAULT,0};

inline arch::x86_64::process_t* __proc_get() {
    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;
    return proc;
}

#define CURRENT_PROC __proc_get()
#define FIND_FD(x) vfs::fdmanager::search(proc,x)

#define SYSCALL_DISABLE_PREEMPT() asm volatile("cli");
#define SYSCALL_ENABLE_PREEMPT() asm volatile("sti");

typedef struct {
    std::int8_t is_rdx_ret;
    std::int32_t ret;
    std::int64_t ret_val;
} syscall_ret_t;

#define STAR_MSR 0xC0000081
#define LSTAR 0xC0000082
#define STAR_MASK 0xC0000084
#define EFER 0xC0000080

#define MAP_FAILED ((void *)(-1))
#define MAP_FILE    0x00
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANON      0x20
#define MAP_ANONYMOUS 0x20

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4

extern "C" void syscall_handler();

struct wiovec { 
void *iov_base;
size_t iov_len;   
}; 

#define FUTEX_WAIT              0
#define FUTEX_WAKE              1
#define FUTEX_FD                2
#define FUTEX_REQUEUE           3
#define FUTEX_CMP_REQUEUE       4
#define FUTEX_WAKE_OP           5
#define FUTEX_LOCK_PI           6
#define FUTEX_UNLOCK_PI         7
#define FUTEX_TRYLOCK_PI        8
#define FUTEX_WAIT_BITSET       9
#define FUTEX_WAKE_BITSET       10
#define FUTEX_WAIT_REQUEUE_PI   11
#define FUTEX_CMP_REQUEUE_PI    12
#define FUTEX_LOCK_PI2          13

#define FUTEX_PRIVATE_FLAG      128
#define FUTEX_CLOCK_REALTIME    256

#define FUTEX_CMD_MASK          ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)

extern "C" {

/* File,Pipes and etc. */
long long sys_access(char* path, int mode);
long long sys_faccessat(int dirfd, char* path, int mode, int flags);
long long sys_openat(int dirfd, const char* path, int flags, int_frame_t* ctx);
long long sys_fstat(int fd, void* out);
long long sys_ioctl(int fd, unsigned long request, void *arg);
long long sys_write(int fd, const void *buf, size_t count);
long long sys_writev(int fd, wiovec* vec, unsigned long vlen);
long long sys_read(int fd, void *buf, size_t count);
long long sys_lseek(int fd, long offset, int whence);
long long sys_dup2(int fd, int newfd);
long long sys_newfstatat(int fd, char* path, void* out, int_frame_t* ctx);
long long sys_dup(int fd);
long long sys_pipe2(int* fildes, int flags);
long long sys_pipe(int* fildes);
long long sys_close(int fd);

long long sys_umask(int mask);

long long sys_symlink(char* old, char* path);
long long sys_symlinkat(char* old, int dirfd, char* path);

long long sys_pread(int fd, void *buf, size_t count, int_frame_t* ctx);

long long sys_getdents64(int fd, char* buf, size_t count);

#define __FD_SETSIZE		1024

typedef long int __fd_mask;

/* Some versions of <linux/posix_types.h> define this macros.  */
#undef	__NFDBITS
/* It's easier to assume 8-bit bytes than to get CHAR_BIT.  */
#define __NFDBITS	(8 * (int) sizeof (__fd_mask))
#define	__FD_ELT(d)	((d) / __NFDBITS)
#define	__FD_MASK(d)	((__fd_mask) (1UL << ((d) % __NFDBITS)))

/* fd_set for select and pselect.  */
typedef struct
  {
    /* XPG4.2 requires this member name.  Otherwise avoid the name
       from the global namespace.  */
#ifdef __USE_XOPEN
    __fd_mask fds_bits[__FD_SETSIZE / __NFDBITS];
# define __FDS_BITS(set) ((set)->fds_bits)
#else
    __fd_mask __fds_bits[__FD_SETSIZE / __NFDBITS];
# define __FDS_BITS(set) ((set)->__fds_bits)
#endif
  } fd_set;

/* We don't use `memset' because this would require a prototype and
   the array isn't too big.  */
#define __FD_ZERO(s) \
  do {									      \
    unsigned int __i;							      \
    fd_set *__arr = (s);						      \
    for (__i = 0; __i < sizeof (fd_set) / sizeof (__fd_mask); ++__i)	      \
      __FDS_BITS (__arr)[__i] = 0;					      \
  } while (0)
#define __FD_SET(d, s) \
  ((void) (__FDS_BITS (s)[__FD_ELT(d)] |= __FD_MASK(d)))
#define __FD_CLR(d, s) \
  ((void) (__FDS_BITS (s)[__FD_ELT(d)] &= ~__FD_MASK(d)))
#define __FD_ISSET(d, s) \
  ((__FDS_BITS (s)[__FD_ELT (d)] & __FD_MASK (d)) != 0)


#define	FD_SET(fd, fdsetp)	__FD_SET (fd, fdsetp)
#define	FD_CLR(fd, fdsetp)	__FD_CLR (fd, fdsetp)
#define	FD_ISSET(fd, fdsetp)	__FD_ISSET (fd, fdsetp)
#define	FD_ZERO(fdsetp)		__FD_ZERO (fdsetp)

long long sys_pselect6(int num_fds, fd_set* read_set, fd_set* write_set, int_frame_t* ctx);
syscall_ret_t sys_create_dev(std::uint64_t requestandnum, char* slave_path, char* master_path);
syscall_ret_t sys_create_ioctl(char* path, std::uint64_t write_and_read_req, std::uint32_t size);
syscall_ret_t sys_setupmmap(char* path, std::uint64_t addr, std::uint64_t size, int_frame_t* ctx);

syscall_ret_t sys_setup_tty(char* path);
syscall_ret_t sys_isatty(int fd);

syscall_ret_t sys_ptsname(int fd, void* out, int max_size);

syscall_ret_t sys_setup_ring_bytelen(char* path, int bytelen);
syscall_ret_t sys_read_dir(int fd, void* buffer);

long long sys_fcntl(int fd, int request, std::uint64_t arg, int_frame_t* ctx);

long long sys_chdir(char* path);
long long sys_fchdir(int fd);

long long sys_poll(struct pollfd *fds, int count, int timeout, int_frame_t* ctx);
long long sys_readlinkat(int dirfd, const char* path, void* buffer, int_frame_t* ctx);

long long sys_readlink(char* path, void* buffer, int max_size);

long long sys_link(char* old_path, char* new_path);
long long sys_linkat(int oldfd, char* old_path, int newfd, int_frame_t* ctx);

long long sys_mkdirat(int dirfd, char* path, int mode);
long long sys_mkdir(char* path, int mode);

long long sys_chmod(char* path, int mode);

syscall_ret_t sys_ttyname(int fd, char *buf, size_t size);
long long sys_rename(char* old, char* newp);

syscall_ret_t sys_eventfd_create(unsigned int initval, int flags);
syscall_ret_t sys_getentropy(char* buffer, std::uint64_t len);

long long sys_pwrite(int fd, void* buf, std::uint64_t n, int_frame_t* ctx);
long long sys_fstatfs(int fd, struct statfs *buf);

long long sys_statfs(char* path, struct statfs* buf);

long long sys_fchmod(int fd, int mode);
long long sys_fchmodat(int dirfd, const char* path, int mode, int_frame_t* ctx);
long long sys_fchmodat2(int dirfd, const char* path, int mode, int_frame_t* ctx);

long long sys_renameat(int olddir, char* old, int newdir, int_frame_t* ctx);
long long sys_statx(int dirfd, const char *path, int flag, int_frame_t* ctx);

/* Process */

long long sys_mmap(std::uint64_t hint, std::uint64_t size, unsigned long prot, int_frame_t* ctx);
long long sys_free(void *pointer, size_t size);
long long sys_exit(int status);

long long sys_exit_group(int status);

long long sys_set_tid_address(int* tidptr);

long long sys_arch_prctl(int option, unsigned long* addr);
long long sys_brk();

long long sys_mprotect(std::uint64_t start, size_t len, std::uint64_t prot);
long long sys_futex(int* uaddr, int op, uint32_t val, int_frame_t* ctx);

#define RLIMIT_CPU		0	/* CPU time in sec */
#define RLIMIT_FSIZE		1	/* Maximum filesize */
#define RLIMIT_DATA		2	/* max data size */
#define RLIMIT_STACK		3	/* max stack size */
#define RLIMIT_CORE		4	/* max core file size */

#ifndef RLIMIT_RSS
# define RLIMIT_RSS		5	/* max resident set size */
#endif

#ifndef RLIMIT_NPROC
# define RLIMIT_NPROC		6	/* max number of processes */
#endif

#ifndef RLIMIT_NOFILE
# define RLIMIT_NOFILE		7	/* max number of open files */
#endif

#ifndef RLIMIT_MEMLOCK
# define RLIMIT_MEMLOCK		8	/* max locked-in-memory address space */
#endif

#ifndef RLIMIT_AS
# define RLIMIT_AS		9	/* address space limit */
#endif

#define RLIMIT_LOCKS		10	/* maximum file locks held */
#define RLIMIT_SIGPENDING	11	/* max number of pending signals */
#define RLIMIT_MSGQUEUE		12	/* maximum bytes in POSIX mqueues */
#define RLIMIT_NICE		13	/* max nice prio allowed to raise to
					   0-39 for nice level 19 .. -20 */
#define RLIMIT_RTPRIO		14	/* maximum realtime priority */
#define RLIMIT_RTTIME		15	/* timeout for RT tasks in us */
#define RLIM_NLIMITS		16

/*
 * SuS says limits have to be unsigned.
 * Which makes a ton more sense anyway.
 *
 * Some architectures override this (for compatibility reasons):
 */
#ifndef RLIM_INFINITY
# define RLIM_INFINITY		(~0UL)
#endif

struct rlimit64 {
	std::uint64_t rlim_cur;
	std::uint64_t rlim_max;
};

long long sys_clone3(clone_args* clargs, size_t size, int c, int_frame_t* ctx);
long long sys_prlimit64(int pid, int res, rlimit64* new_rlimit, int_frame_t* ctx);

syscall_ret_t sys_iopl(int a, int b ,int c , int_frame_t* ctx);

syscall_ret_t sys_fork(int D, int S, int d, int_frame_t* ctx);

syscall_ret_t sys_access_framebuffer(void* out);

long long sys_exec(char* path, char** argv, char** envp, int_frame_t* ctx);

long long sys_getpid();
long long sys_getppid();
long long sys_getpgrp();

long long sys_setpgid(int pid, int pgid);
long long sys_getpgid(int pid);

syscall_ret_t sys_gethostname(void* buffer, std::uint64_t bufsize);

long long sys_getcwd(void* buffer, std::uint64_t bufsize);

long long sys_wait4(int pid,int* status,int flags);

syscall_ret_t sys_sleep(long us);

syscall_ret_t sys_alloc_dma(std::uint64_t size);
syscall_ret_t sys_free_dma(std::uint64_t phys);

syscall_ret_t sys_map_phys(std::uint64_t phys, std::uint64_t flags, std::uint64_t size);

syscall_ret_t sys_timestamp();

syscall_ret_t sys_mkfifoat(int dirfd, const char *path, int mode);

syscall_ret_t sys_enabledebugmode();

long long sys_clone(unsigned long clone_flags, unsigned long newsp, int *parent_tidptr, int_frame_t* ctx);
syscall_ret_t sys_breakpoint(int num, int b, int c, int_frame_t* ctx);

long long sys_getpriority(int which, int who);
long long sys_setpriority(int which, int who, int prio);

syscall_ret_t sys_yield();
syscall_ret_t sys_printdebuginfo(int pid);
syscall_ret_t sys_enabledebugmodepid(int pid);

syscall_ret_t sys_dmesg(char* buf,std::uint64_t count);

long long sys_getuid();

long long sys_getresuid(int* ruid, int* euid, int *suid);

long long sys_setuid(int uid);

long long sys_kill(int pid, int sig);
long long sys_tgkill(int tgid, int tid, int sig);

/* Futex */
syscall_ret_t sys_futex_wait(int* pointer, int excepted, struct timespec* ts);
syscall_ret_t sys_futex_wake(int* pointer);

/* Socket */
long long sys_socket(int family, int type, int protocol);

long long sys_bind(int fd, struct sockaddr_un* path, int len);
long long sys_accept(int fd, struct sockaddr_un* path, int* zlen);
long long sys_connect(int fd, struct sockaddr_un* path, int len);

long long sys_listen(int fd, int backlog);
long long sys_sendto(int s, void* msg, size_t len);

syscall_ret_t sys_copymemory(void* src, void* dest, int len);

syscall_ret_t sys_socketpair(int domain, int type_and_flags, int proto);
long long sys_getsockname(int fd, struct sockaddr_un* path, int* len);

long long sys_getsockopt(int fd, int layer, int number, int_frame_t* ctx);

long long sys_msg_recv(int fd, struct msghdr *hdr, int flags);
syscall_ret_t sys_msg_send(int fd, struct msghdr* hdr, int flags);

long long sys_shutdown(int sockfd, int how);

/* Shm stuff */

syscall_ret_t sys_shmctl(int shmid, int cmd, struct shmid_ds *buf);
syscall_ret_t sys_shmat(int shmid, std::uint64_t hint, int shmflg);
syscall_ret_t sys_shmget(int key, size_t size, int shmflg);
syscall_ret_t sys_shmdt(std::uint64_t base);

/* Signals */

typedef unsigned long __cpu_mask;

#define __CPU_SETSIZE 1024
#define __NCPUBITS (8 * sizeof(__cpu_mask))

typedef struct {
	__cpu_mask __bits[__CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

long long sys_pause();

long long sys_sigaction(int signum, struct sigaction* hnd, struct sigaction* old, int_frame_t* ctx);
long long sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, int_frame_t* ctx);

typedef int pid_t;

syscall_ret_t sys_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);

syscall_ret_t sys_cpucount();

syscall_ret_t sys_pathstat(char* path, void* out, int flags, int_frame_t* ctx);

long long sys_alarm(int seconds);
long long sys_setitimer(int which, itimerval* val, itimerval* old);
long long sys_getitimer(int which, itimerval* val);

long long sys_sigsuspend(sigset_t* sigset, size_t size);
long long sys_sigaltstack(stack_t* new_stack, stack_t* old);

long long sys_unlink(char* path);
long long sys_unlinkat(int dirfd, const char* path, int flags);

/* Misc */

struct __kernel_timespec {
	std::uint64_t       tv_sec;                 /* seconds */
	long long               tv_nsec;                /* nanoseconds */
};

typedef int clockid_t;

struct timeval {
        long long    tv_sec;         /* seconds */
        long long    tv_usec;        /* microseconds */
};


long long sys_getrandom(char *buf, size_t count, unsigned int flags);
long long sys_clock_gettime(clockid_t which_clock, struct __kernel_timespec *tp);
long long sys_gettimeofday(timeval* tv, void* tz);
long long sys_time(std::uint64_t* t);

long long sys_nanosleep(int clock, int flags, timespec* rqtp, int_frame_t* ctx);

#define TIMER_ABSTIME                   0x01

struct old_utsname {
        char sysname[65];
        char nodename[65];
        char release[65];
        char version[65];
        char machine[65];
};

long long sys_uname(old_utsname* uname);

struct sysinfo {
        long uptime;             /* Количество секунд, прошедшее с загрузки системы */
        unsigned long loads[3];  /* средняя одно-, пяти-, и пятнадцатиминутная загруженность системы */
        unsigned long totalram;  /* Общий объем доступной оперативной памяти */
        unsigned long freeram;   /* Объем свободной памяти */
        unsigned long sharedram; /* Объем разделяемой памяти */
        unsigned long bufferram; /* Объем памяти, использованной под буферы */
        unsigned long totalswap; /* Общий объем области подкачки */
        unsigned long freeswap;  /* Объем свободного пространства в области подкачки */
        unsigned short procs;    /* Текущее количество процессов */
        unsigned long totalhigh; /* Общий объем верхней памяти */
        unsigned long freehigh;  /* Объем свободной верхней памяти */
        unsigned int mem_unit;   /* Объем единицы памяти в байтах */
        char _f[20-2*sizeof(long)-sizeof(int)]; /* Дополнение структуры для libc5 */
};

long long sys_sysinfo(sysinfo* info);

}

struct pollfd {
    int fd;          
    short events;   
    short revents;  
};

namespace arch {
    namespace x86_64 {
        typedef struct {
            int syscall_num;
            void* syscall_func;
        } syscall_item_t;
        class syscall {
        public:
            static void init();
        };
    }
}
