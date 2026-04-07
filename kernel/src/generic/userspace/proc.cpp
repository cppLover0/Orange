#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <utils/linux.hpp>
#include <generic/userspace/safety.hpp>

#if defined(__x86_64__)
#define ARCH_SET_GS                     0x1001
#define ARCH_SET_FS                     0x1002
#define ARCH_GET_FS                     0x1003
#define ARCH_GET_GS                     0x1004
#define ARCH_GET_CPUID                  0x1011
#define ARCH_SET_CPUID                  0x1012
#include <arch/x86_64/assembly.hpp>
#endif

long long sys_arch_prctl(int option, unsigned long* addr) {
#if defined(__x86_64__)
    if(!addr)
        return -EINVAL;

    thread* proc = current_proc;

    switch(option) {
    case ARCH_SET_FS: 
        proc->fs_base = (std::uint64_t)addr;
        assembly::wrmsr(0xC0000100,(std::uint64_t)addr);
        return 0;
    case ARCH_GET_FS:
        if(!is_safe_to_rw(proc, (std::uint64_t)addr, 4096))
            return -EFAULT;
        *addr = proc->fs_base;
        return 0;
    default:
        assert(0, "unsupported prctl option 0x%p", option);
        return -EINVAL;
    };

    return 0;
#else
#error "prctl for other arches is not implemented !"
#endif
}