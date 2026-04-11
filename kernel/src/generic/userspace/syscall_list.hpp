#pragma once
#include <cstdint>
#include <generic/vfs.hpp>
#include <klibc/string.hpp>
#include <utils/assert.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/cpu_local.hpp>
#define current_proc (CPU_LOCAL_READ(current_thread))
#else
#error "todo"
#endif

inline static void process_path(const char* chroot, const char* at, const char* path, char* result) {
    size_t chroot_len = (chroot ? klibc::strlen(chroot) : 0);
    size_t at_len = klibc::strlen(at);
    size_t path_len = klibc::strlen(path);
    
    assert(chroot_len + at_len + path_len + 10 < 4096, "Path too long");

    char result0[4096] = {};
    
    vfs::resolve_path((char*)path, (char*)at, result0, 1);

    size_t offset = 0;
    if (chroot && chroot_len > 0) {
        klibc::memcpy(result, chroot, chroot_len);
        offset = chroot_len;

        if (result[offset - 1] != '/' && result0[0] != '/') {
            result[offset++] = '/';
        }
    }

    const char* src = result0;
    if (chroot && result0[0] == '/') src++; 

    klibc::memcpy(result + offset, src, klibc::strlen(src) + 1);
    return;
}

inline static char* at_to_char(thread* thread, int atdir) {
    if(atdir == -100) {
        return thread->cwd;
    } else if(atdir > 0) {
        vfs::fdmanager* manager = (vfs::fdmanager*)thread->fd;
        file_descriptor* fd = manager->search(atdir);
        if(!fd)
            return nullptr;
        return fd->path;
    }
    return nullptr;
}

long long sys_setpgid(int pid, int pgid);
long long sys_dup2(int old, int new_fd);
long long sys_dup(int fd);
long long sys_getuid();
long long sys_getgid();
long long sys_getpid();
long long sys_getppid();
long long sys_getpgrp();

long long sys_seek(int fd, long offset, int whence);
long long sys_getresgid(int* uid, int* euid, int* suid);
long long sys_getresuid(int* uid, int* euid, int* suid);
long long sys_munmap(std::uint64_t addr, std::uint64_t len);
long long sys_pselect6(int num_fds, fd_set* read_set, fd_set* write_set, fd_set* except_set, timespec* timeout, sigset_t* sigmask);
long long sys_poll(pollfd* fds, std::uint32_t nfds, int timeout);
long long sys_time(std::uint64_t* time);
long long sys_write(int fd, char* buffer, std::uint64_t count);
long long sys_pipe2(int* fds, int flags);
long long sys_faccessat2(int dfd, const char* path, int mode, int flags);
long long sys_fcntl(int fd, int request, std::uint64_t arg);
long long sys_uname(old_utsname* utsname);
long long sys_getcwd(char* buf, std::uint64_t len);
long long sys_rseq(void* rseq, std::uint32_t len, int flags, std::uint32_t sig);
long long sys_set_robust_list(void* head, std::uint64_t len);
long long sys_get_robust_list(int pid, void* head, std::uint64_t* len);
long long sys_clone3(void* ctx, clone_args* clarg, std::uint64_t size);
long long sys_clone(void* frame, unsigned long flags, unsigned long newsp, int* parent_tidptr, int* child_tidptr, unsigned long tls);
long long sys_gettimeofday(timeval* tv, void* tz);
long long sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, std::uint64_t len);
long long sys_sigprocaction(int sig, const sigaction* src, sigaction* old, std::uint64_t len);

long long sys_exit_group(int status);
long long sys_clock_gettime(clockid_t which_clock, struct timespec *tp);
long long sys_wait4();
long long sys_writev(int fd, iovec* vecs, std::uint64_t vlen);
long long sys_fcntl(int fd, int request, std::uint64_t arg);
long long sys_close(int fd);
long long sys_brk(std::uint64_t brk);
long long sys_fstat(int fd, stat* out);
long long sys_set_tid_address(int* tidptr);
long long sys_access(const char* path, int mode);
long long sys_arch_prctl(int option, unsigned long* addr);
long long sys_read(int fd, char* buffer, std::uint64_t count);
long long sys_readlink(const char* path, char* buf, int size);
long long sys_ioctl(int fd, std::uint64_t req, std::uint64_t arg);
long long sys_openat(int dfd, const char* path, int flags, int mode);
long long sys_newfstatat(int dfd, const char* path, stat* out, int flags);
long long sys_readlinkat(int dfd, const char* path, char* buf, int bufsize);
long long sys_getrandom(char* buf, std::uint64_t count, std::uint32_t flags);
long long sys_pread64(int fd, char* buffer, std::uint64_t count, std::uint64_t pos);
long long sys_prlimit64(int pid, int res, rlimit64* new_rlimit, rlimit64* old_rlimit);
long long sys_mmap(std::uint64_t hint, std::uint64_t len, std::uint64_t prot, std::uint64_t flags, int fd, std::uint64_t off);