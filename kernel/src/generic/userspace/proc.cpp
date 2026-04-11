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

long long sys_set_robust_list(void* head, std::uint64_t len) {
    thread* proc = current_proc;
    if(len != sizeof(robust_list_head))
        return -EINVAL;    
    
    if(!is_safe_to_rw(proc, (std::uint64_t)head, len))
        return -EFAULT;

    proc->robust = (robust_list_head*)head;
    return 0;
}

long long sys_get_robust_list(int pid, void* head, std::uint64_t* len) {
    thread* proc = nullptr;
    if(pid > 0) 
        proc = process::by_id(pid);
    else if(pid == 0)
        proc = current_proc;
    else
        return -EINVAL;

    if(proc == nullptr)
        return -ESRCH;
        
    if(!is_safe_to_rw(current_proc, (std::uint64_t)head, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(current_proc, (std::uint64_t)len, PAGE_SIZE))
        return -EFAULT;

    *(std::uintptr_t*)head = (std::uint64_t)proc->robust;
    *len = sizeof(robust_list_head);

    return 0;
}

long long sys_rseq(void* rseq, std::uint32_t len, int flags, std::uint32_t sig) {
    (void)rseq;
    (void)len;
    (void)flags;
    (void)sig;
    return -ENOSYS;
}

long long sys_prlimit64(int pid, int res, rlimit64* new_rlimit, rlimit64* old_rlimit) {

    (void)pid;
    if(new_rlimit || !old_rlimit)
        return 0;

    switch(res) {
        case RLIMIT_NOFILE:
            old_rlimit->rlim_cur = 512*1024;
            old_rlimit->rlim_max = 512*1024;
            return 0;
        case RLIMIT_STACK:
            old_rlimit->rlim_cur = (4 * 1024 * 1024) - PAGE_SIZE;
            old_rlimit->rlim_max = (4 * 1024 * 1024) - PAGE_SIZE;
            return 0;
        case RLIMIT_NPROC:
            old_rlimit->rlim_cur = 0xffffffff;
            old_rlimit->rlim_max = 0xffffffff;
            return 0;
        case RLIMIT_AS:
        case RLIMIT_LOCKS:
        case RLIMIT_MEMLOCK:
        case RLIMIT_CPU:
        case RLIMIT_RSS:
        case RLIMIT_FSIZE:
        case RLIMIT_DATA:
            old_rlimit->rlim_cur = RLIM_INFINITY;
            old_rlimit->rlim_max = RLIM_INFINITY;
            return 0;
        case RLIMIT_CORE:
            old_rlimit->rlim_cur = 0;
            old_rlimit->rlim_max = 0;
            return 0;
    default:
        return -EINVAL;
    }
    return 0;
}

long long sys_getuid() {
    return current_proc->uid;
}

long long sys_getgid() {
    return current_proc->gid;
}

long long sys_getcwd(char* buf, std::uint64_t len) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)buf, PAGE_SIZE)) {
        return -EFAULT;
    }

    if(buf == nullptr)
        return -EINVAL;

    if(len < (std::uint64_t)klibc::strlen(current->cwd) + 1)
        return -ERANGE;

    klibc::memset(buf, 0, len);
    klibc::memcpy(buf, current->cwd, klibc::strlen(current->cwd) + 1);

    return klibc::strlen(current->cwd) + 1;
}

long long sys_getpid() {
    return current_proc->pid;
}

long long sys_getppid() {
    return current_proc->parent_pid;
}

long long sys_getpgrp() {
    return current_proc->pgrp;
}

long long sys_setpgid(int pid, int pgid) {
    thread* proc = nullptr;
    if(pid > 0) 
        proc = process::by_id(pid);
    else if(pid == 0)
        proc = current_proc;
    else
        return -EINVAL;

    if(proc == nullptr)
        return -ESRCH;

    proc->pgrp = pgid;
    return 0;
}

long long sys_getresuid(int* uid, int* euid, int* suid) {
    thread* proc = current_proc;

    if(!is_safe_to_rw(proc, (std::uint64_t)uid, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)euid, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)suid, PAGE_SIZE))
        return -EFAULT;

    if(!uid || !euid || !suid)
        return -EINVAL;

    *uid = proc->uid;
    *euid = proc->uid;
    *suid = proc->uid;
        
    return 0;
}

long long sys_getresgid(int* uid, int* euid, int* suid) {

    thread* proc = current_proc;

    if(!is_safe_to_rw(proc, (std::uint64_t)uid, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)euid, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)suid, PAGE_SIZE))
        return -EFAULT;

    if(!uid || !euid || !suid)
        return -EINVAL;

    *uid = proc->gid;
    *euid = proc->gid;
    *suid = proc->gid;
        
    return 0;
}

long long sys_wait4() {
    while(1) {
        process::yield();
    }
}

void debug_print_clone_flags(unsigned long flags) {
    klibc::debug_printf("clone3 flags (0x%lx): ", flags);
    
    if (flags & 0x00000100) klibc::debug_printf("CLONE_VM ");
    if (flags & 0x00000200) klibc::debug_printf("CLONE_FS ");
    if (flags & 0x00000400) klibc::debug_printf("CLONE_FILES ");
    if (flags & 0x00000800) klibc::debug_printf("CLONE_SIGHAND ");
    if (flags & 0x00001000) klibc::debug_printf("CLONE_PIDFD ");
    if (flags & 0x00002000) klibc::debug_printf("CLONE_PTRACE ");
    if (flags & 0x00004000) klibc::debug_printf("CLONE_VFORK ");
    if (flags & 0x00008000) klibc::debug_printf("CLONE_PARENT ");
    if (flags & 0x00010000) klibc::debug_printf("CLONE_THREAD ");
    if (flags & 0x00020000) klibc::debug_printf("CLONE_NEWNS ");
    if (flags & 0x00040000) klibc::debug_printf("CLONE_SYSVSEM ");
    if (flags & 0x00080000) klibc::debug_printf("CLONE_SETTLS ");
    if (flags & 0x00100000) klibc::debug_printf("CLONE_PARENT_SETTID ");
    if (flags & 0x00200000) klibc::debug_printf("CLONE_CHILD_CLEARTID ");
    if (flags & 0x00400000) klibc::debug_printf("CLONE_DETACHED ");
    if (flags & 0x00800000) klibc::debug_printf("CLONE_UNTRACED ");
    if (flags & 0x01000000) klibc::debug_printf("CLONE_CHILD_SETTID ");
    if (flags & 0x02000000) klibc::debug_printf("CLONE_NEWCGROUP ");
    if (flags & 0x04000000) klibc::debug_printf("CLONE_NEWUTS ");
    if (flags & 0x08000000) klibc::debug_printf("CLONE_NEWIPC ");
    if (flags & 0x10000000) klibc::debug_printf("CLONE_NEWUSER ");
    if (flags & 0x20000000) klibc::debug_printf("CLONE_NEWPID ");
    if (flags & 0x40000000) klibc::debug_printf("CLONE_NEWNET ");
    if (flags & 0x80000000) klibc::debug_printf("CLONE_IO ");
    
    klibc::debug_printf("\n");
}

long long clone3_impl(void* ctx, clone_args* clarg, std::uint64_t size) {
    (void)size;

    thread* proc = current_proc;

    assert(ctx, "no 3");

    if(proc->is_debug) {
        debug_print_clone_flags(clarg->flags);
    }

    thread* new_proc = process::clone3(proc, *clarg, ctx);
#if defined(__x86_64__)
    new_proc->ctx.rax = 0;
#endif
    process::wakeup(new_proc);
    new_proc->is_debug = proc->is_debug;

    if(clarg->flags & CLONE_VFORK) {
        while(1) {
            if(new_proc->did_exec == false)
                break;
            process::yield();    
        }
    }

    return new_proc->id;
}

long long sys_clone3(void* ctx, clone_args* clarg, std::uint64_t size) {
    (void)size;

    log("t", "type shit 0x%p 0x%p %lli", ctx, clarg, size);

    thread* proc = current_proc;
    if(!is_safe_to_rw(proc, (std::uint64_t)clarg, PAGE_SIZE))
        return -EFAULT;

    if(clarg == nullptr)
        return -EINVAL;

    return clone3_impl(ctx, clarg, size);
}


long long sys_clone(void* frame, unsigned long flags, unsigned long newsp, int* parent_tidptr, int* child_tidptr, unsigned long tls) {
    clone_args arg = {};
    
    arg.exit_signal = flags & 0xFF; 
    arg.flags = flags & ~0xFF; 
    
    arg.stack = newsp;
    arg.parent_tid = (std::uintptr_t)parent_tidptr;
    arg.child_tid = (std::uintptr_t)child_tidptr;
    arg.tls = tls;

    return clone3_impl(frame, &arg, sizeof(clone_args));
}

long long sys_exit_group(int status) {
    thread* proc = current_proc;
    proc->exit_request = 2;
    proc->exit_code = status;
    arch::enable_paging(gobject::kernel_root);
    process::yield();
    __builtin_unreachable();
}