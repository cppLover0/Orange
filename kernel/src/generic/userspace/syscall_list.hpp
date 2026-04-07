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

long long sys_close(int fd);
long long sys_brk(std::uint64_t brk);
long long sys_fstat(int fd, stat* out);
long long sys_set_tid_address(int* tidptr);
long long sys_access(const char* path, int mode);
long long sys_arch_prctl(int option, unsigned long* addr);
long long sys_read(int fd, char* buffer, std::uint64_t count);
long long sys_openat(int dfd, const char* path, int flags, int mode);
long long sys_newfstatat(int dfd, const char* path, stat* out, int flags);
long long sys_pread64(int fd, char* buffer, std::uint64_t count, std::uint64_t pos);
long long sys_mmap(std::uint64_t hint, std::uint64_t len, std::uint64_t prot, std::uint64_t flags, int fd, std::uint64_t off);