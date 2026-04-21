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
#include <generic/heap.hpp>
#include <generic/elf.hpp>
#include <klibc/string.hpp>
#include <generic/time.hpp>

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

long long sys_getpgid(int pid) {
    thread* proc = nullptr;
    if(pid > 0) 
        proc = process::by_id(pid);
    else if(pid == 0)
        proc = current_proc;
    else
        return -EINVAL;

    if(proc == nullptr)
        return -ESRCH;

    return proc->pgrp;
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

long long sys_getgid() {
    return current_proc->gid;
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

#define WNOHANG 1

long long sys_wait4(int pid, int *wstatus, int options) {
    thread* proc = current_proc;

    thread* current = process::_head_proc();

    if(proc->is_debug) klibc::debug_printf("Trying to waitpid with pid %d from proc %d",pid,proc->id);
    
    if(pid < -1 || pid == 0)
        return -EINVAL;

    if(!is_safe_to_rw(proc, (std::uint64_t)wstatus, 4096))
        return -EFAULT;

    int success = 0;

    if(pid == -1) {
        while (current)
        {
            if(current->parent_pid == proc->id && (current->status == PROCESS_ZOMBIE || current->status == PROCESS_LIVE) && current->waitpid_state != 2) {
                current->waitpid_state = 1;
                success = 1;
            }
            current = current->next;
        }
    } else if(pid > 0) {
        while (current)
        {
            if(current->parent_pid == proc->id && (current->status == PROCESS_ZOMBIE || current->status == PROCESS_LIVE) && current->id == (std::uint32_t)pid && current->waitpid_state != 2) {
                current->waitpid_state = 1;
                success = 1;
                break;
            }
            current = current->next;
        }
    }

    if(!success) 
        return -ECHILD;

    proc = current_proc;

    std::uint64_t stack_protect = 0xFFAACCBB11009922;
    std::uint32_t parent_id = proc->id;

    current = process::_head_proc();
    while(1) {
        while (current)
        {
            if(current) {
                if(current->parent_pid == parent_id && current->status == PROCESS_ZOMBIE && current->waitpid_state == 1 && current->id != 0) {
                    klibc::debug_printf("%d %d x%d x \n", current_proc->id, proc->id, current->parent_pid);
                    current->waitpid_state = 2;
                    std::int64_t bro = (std::int64_t)(((std::uint64_t)current->exit_code) << 32) | current->id;
                    current->status = PROCESS_KILLED;
                    if(proc->is_debug) klibc::debug_printf("Waitpid done pid %d from proc %d",pid,proc->id);
                    if(wstatus)  
                        *wstatus = current->exit_code;
                    return bro;
                } 
            }
            assert(stack_protect == 0xFFAACCBB11009922, "stack fucked :(");
            current = current->next;
        }

        if(options & WNOHANG) {
            thread* pro = process::_head_proc();
            while(pro) {
                if(pro->parent_pid == parent_id && proc->waitpid_state == 1)
                    proc->waitpid_state = 0;
                pro = pro->next;
            }
            if(proc->is_debug) klibc::debug_printf("Waitpid return WNOHAND from proc %d",proc->id);
            return 0;
        }
        
        process::yield();
        current = process::_head_proc();
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
            if(new_proc->did_exec == true)
                break;
            process::yield();    
        }
    }

    return new_proc->id;
}

long long sys_clone3(void* ctx, clone_args* clarg, std::uint64_t size) {
    (void)size;

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

long long sys_newthread(void* frame, std::uint64_t new_ip, std::uint64_t new_stack) {

    thread* current_thread = current_proc;

    clone_args clarg = {};
    clarg.flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
		| CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS;
    clarg.stack = new_stack;

    thread* new_proc = process::clone3(current_thread, clarg, frame);

#if defined(__x86_64__)
    new_proc->ctx.rax = 0;
    new_proc->ctx.rip = new_ip;
    new_proc->ctx.rsp = new_stack;
#else
#error "sh"
#endif

    process::wakeup(new_proc);
    return new_proc->id;
}

long long sys_exit_group(int status) {
    thread* proc = current_proc;
    proc->exit_request = 2;
    proc->exit_code = (status & 0xFF) << 8;
    arch::enable_paging(gobject::kernel_root);
    process::yield();
    while(1) {process::yield();}
    __builtin_unreachable();
}

long long sys_exit(int status) {
    thread* proc = current_proc;
    proc->exit_request = 1;
    proc->exit_code = (status & 0xFF) << 8;
    arch::enable_paging(gobject::kernel_root);
    while(1) {process::yield();}
    __builtin_unreachable();
}

inline static long long get_array_len(char** arr) {
    uint64_t counter = 0;

    while(arr[counter]) {
        counter++;
        char* t = (char*)(arr[counter - 1]);
        if(*t == '\0')
            return counter - 1; // invalid
    } 

    return counter;
}

long long sys_execve(const char* path, char** argv, char** envp) {
    thread* current_thread = current_proc;

    if(!is_safe_to_rw(current_thread,(std::uint64_t)argv,PAGE_SIZE * 256))
        return -EFAULT;

    if(!is_safe_to_rw(current_thread,(std::uint64_t)envp,PAGE_SIZE * 256))
        return -EFAULT;

    if(!is_safe_to_rw(current_thread,(std::uint64_t)path,PAGE_SIZE * 256))
        return -EFAULT;

    std::uint64_t argv_len = get_array_len(argv);
    std::uint64_t envp_len = get_array_len(envp);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
    char* stack_argv[argv_len + 1];
    klibc::memset(stack_argv, 0, sizeof(stack_argv));

    char* stack_envp[envp_len + 1];
    klibc::memset(stack_envp, 0, sizeof(stack_envp));
#pragma clang diagnostic pop

    for(std::uint64_t i = 0;i < argv_len; i++) {

        char* str = argv[i];

        char* new_str = (char*)kheap::malloc(klibc::strlen(str) + 1);

        klibc::memcpy(new_str,str,klibc::strlen(str) + 1);

        stack_argv[i] = new_str;

    }

    for(std::uint64_t i = 0;i < envp_len; i++) {

        char* str = envp[i];

        char* new_str = (char*)kheap::malloc(klibc::strlen(str) + 1);

        klibc::memcpy(new_str,str,klibc::strlen(str));

        stack_envp[i] = new_str;

    }

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current_thread, AT_FDCWD);
    if(at == nullptr)
        return -EBADF;

    process_path(current_proc->chroot, at, buffer1, buffer);

    file_descriptor file = {};

    int status = vfs::open(&file, buffer, true, false);

    if(status != 0)
        return status;

    stat file_stat = {};

    file.vnode.stat(&file, &file_stat);
    if(!(file_stat.st_mode & S_IFREG) || !(file_stat.st_mode & S_IXUSR))
        return -ENOEXEC;

    char* bufferfv = (char*)(pmm::buddy::alloc(file_stat.st_size).phys + etc::hhdm());
    file.vnode.read(&file, (char*)bufferfv, file_stat.st_size);

    if(file.vnode.close)
        file.vnode.close(&file);

    bool is_valid = elf::is_valid_elf(current_thread, (char*)bufferfv);

    pmm::buddy::free((std::uint64_t)bufferfv - etc::hhdm());
    if(!is_valid) 
        return -ENOEXEC;

    current_thread->should_not_save_ctx = true;
    current_thread->did_exec = true;
    
    arch::enable_paging(gobject::kernel_root);

    vfs::fdmanager* manager = (vfs::fdmanager*)current_thread->fd;
    manager->cloexec();

    current_thread->vmem->free();

    current_thread->original_root = pmm::freelist::alloc_4k();

    current_thread->vmem = new vmm;
    current_thread->vmem->root = current_thread->original_root;
    current_thread->vmem->init_root();
    current_thread->should_not_save_ctx = true;
    
    klibc::memset(&current_thread->ctx, 0, sizeof(current_thread->ctx));

#if defined(__x86_64__)
    bool is_user = true;
    current_thread->ctx.ss = is_user ? (0x18 | 3) : 0;
    current_thread->ctx.cs = is_user ? (0x20 | 3) : 0x08;
    current_thread->ctx.rflags = (1 << 9);
    current_thread->ctx.cr3 = current_thread->original_root;
#endif

    elf::exec(current_thread, buffer, stack_argv, stack_envp);

    for(std::uint64_t i = 0;i < argv_len; i++) {
        kheap::free(stack_argv[i]);
    }

    for(std::uint64_t i = 0;i < envp_len; i++) {
        kheap::free(stack_envp[i]);
    }

    process::yield();

    assert(0, "wtf !!?!?!?!?!?");
    __builtin_unreachable();
    return -ENOEXEC;
}

long long sys_gettid() {
    return current_proc->id;
}

#define P_ALL           0
#define P_PID           1
#define P_PGID          2
#define P_PIDFD         3

long long sys_waitid(int which, int pid, siginfo* siginfo, int options, void* ru) {

    (void)ru;
    if(!is_safe_to_rw(current_proc, (std::uint64_t)siginfo, PAGE_SIZE))
        return -EFAULT;

    if(siginfo == nullptr)
        return -EINVAL;

    int waitpid_which = 0;
    switch(which) {
    case P_PID:
        waitpid_which = pid;
    case P_PGID:
        waitpid_which = -pid;
    case P_ALL:
        waitpid_which = -1;
    default:
        assert(0,"waitid unimplemented which %d", which);
    }

    int res = sys_wait4(waitpid_which, &siginfo->si_code, options);
    siginfo->si_code = (siginfo->si_code >> 8) & 0xFF;
    return res;
}

long long sys_yield() {
    process::yield();
    return 0;
}