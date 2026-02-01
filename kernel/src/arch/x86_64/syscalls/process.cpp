
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

syscall_ret_t sys_tcb_set(std::uint64_t fs) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->fs_base = fs;
    __wrmsr(0xC0000100,fs);
    DEBUG(proc->is_debug,"Setting tcb %p to proc %d",fs,proc->id);
    return {0,0,0};
}

syscall_ret_t sys_libc_log(const char* msg) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    char buffer[2048];
    memset(buffer,0,2048);
    copy_in_userspace_string(proc,buffer,(void*)msg,2048);
    DEBUG(proc->is_debug,"%s from proc %d",buffer,proc->id);

    return {0,0,0};
}

long long sys_exit_group(int status) {
    cpudata_t* cpdata = arch::x86_64::cpu::data();
    arch::x86_64::process_t* proc = CURRENT_PROC;
    DEBUG(1,"Process %d exited with status %d (exit_group)",proc->id,status);
    char* vm_start = proc->vmm_start;
    arch::x86_64::process_t* current = arch::x86_64::scheduling::head_proc_();
    memory::paging::enablekernel();
    DEBUG(proc->is_debug,"a");
    while(current) {
        if(proc->id == current->id) {
            current->exit_code = 0;
            DEBUG(proc->is_debug,"sh %d",current->id);
            arch::x86_64::scheduling::kill(current);
            
            if(1)
                memory::vmm::free(current); 

            vfs::fdmanager* fd = (vfs::fdmanager*)current->fd;
            fd->free();

            memory::pmm::_virtual::free(current->cwd);
            memory::pmm::_virtual::free(current->name);
            memory::pmm::_virtual::free(current->sse_ctx);
            cpdata->temp.temp_ctx = 0;
        } else if(proc->vmm_start == current->vmm_start && proc->thread_group == current->thread_group) {
            // send sigkill
            if(current->sig) {
                pending_signal_t sig;
                sig.sig = SIGKILL;
                current->sig->push(&sig);
            }
        }
        current = current->next;
    }
    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
    __builtin_unreachable();
}

long long sys_exit(int status) {

    cpudata_t* cpdata = arch::x86_64::cpu::data();

    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->exit_code = status;

    if(proc->tidptr) {
        *proc->tidptr = 0;
        arch::x86_64::scheduling::futexwake(proc,proc->tidptr,1);
    }


    memory::paging::enablekernel();

    DEBUG(1,"Process %s (%d) exited with code %d sys %d",proc->name,proc->id,status,proc->sys);

    if(proc->is_debug)
        assert(1,"debug: proc exit status is not 0");

    if(proc->exit_signal != 0) {
        arch::x86_64::process_t* target_proc = arch::x86_64::scheduling::by_pid(proc->parent_id);
        if(target_proc) {
            pending_signal_t pend_sig;
            pend_sig.sig = proc->exit_signal;
            target_proc->sig->push(&pend_sig);
        }
    }

    arch::x86_64::scheduling::kill(proc);
    
    if(1)
        memory::vmm::free(proc); 

    vfs::fdmanager* fd = (vfs::fdmanager*)proc->fd;
    fd->free();

    memory::pmm::_virtual::free(proc->cwd);
    memory::pmm::_virtual::free(proc->name);
    memory::pmm::_virtual::free(proc->sse_ctx);
    cpdata->temp.temp_ctx = 0;
    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
    __builtin_unreachable();
}

locks::spinlock mmap_lock; // memory access should be locked anyway 

#define PROT_READ	0x1		/* page can be read */
#define PROT_WRITE	0x2		/* page can be written */
#define PROT_EXEC	0x4		/* page can be executed */
#define PROT_SEM	0x8		/* page may be used for atomic ops */
/*			0x10		   reserved for arch-specific use */
/*			0x20		   reserved for arch-specific use */
#define PROT_NONE	0x0		/* page can not be accessed */
#define PROT_GROWSDOWN	0x01000000	/* mprotect flag: extend change to start of growsdown vma */
#define PROT_GROWSUP	0x02000000	/* mprotect flag: extend change to end of growsup vma */

// unimplemented just return errors if not valid
long long sys_mprotect(std::uint64_t start, size_t len, std::uint64_t prot) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(!start)
        return -EINVAL;

    vmm_obj_t* vmm_ = memory::vmm::getlen(proc,start);
    
    if(!vmm_)
        return -EINVAL;

    DEBUG(proc->is_debug,"trying to mprotect 0x%p",start);

    std::uint64_t new_flags = PTE_USER | PTE_PRESENT;
    new_flags |= (prot & PROT_WRITE) ? PTE_RW : 0;

    return 0;
}

long long sys_prlimit64(int pid, int res, rlimit64* new_rlimit, int_frame_t* ctx) {
    rlimit64* old_rlimit = (rlimit64*)ctx->r10;
    
    if(new_rlimit || !old_rlimit)
        return 0;

    switch(res) {
        case RLIMIT_NOFILE:
            old_rlimit->rlim_cur = 512*1024;
            old_rlimit->rlim_max = 512*1024;
            return 0;
        case RLIMIT_STACK:
            old_rlimit->rlim_cur = USERSPACE_STACK_SIZE;
            old_rlimit->rlim_max = USERSPACE_STACK_SIZE;
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

long long sys_mmap(std::uint64_t hint, std::uint64_t size, unsigned long prot, int_frame_t* ctx) {

    SYSCALL_DISABLE_PREEMPT(); // ensure interrupts disabled
    mmap_lock.lock();

    std::uint64_t flags = ctx->r10;
    int fd0 = ctx->r8;
    std::uint64_t off = ctx->r9;

    size = ALIGNPAGEUP(size);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    
    DEBUG(proc->is_debug,"trying to mmap 0x%p-0x%p sz %lli, prot %lli, flags %lli fd %d off %lli from proc %d",hint,hint + size,size,prot,flags,fd0,off,proc->id);
    
    if(flags & MAP_ANONYMOUS) {

        std::uint64_t new_hint = hint;
        int is_shared = (flags & MAP_SHARED) ? 1 : 0;

        if(is_shared) {
            DEBUG(1,"shared mem\n");
            asm volatile("hlt");
        }

        if(!new_hint) {
            new_hint = (std::uint64_t)memory::vmm::lazy_alloc(proc,size,PTE_PRESENT | PTE_USER | PTE_RW,0);
        } else 
            memory::vmm::customalloc(proc,new_hint,size,PTE_PRESENT | PTE_RW | PTE_USER,0,0);

        memory::paging::enablepaging(ctx->cr3); // try to reset tlb

        mmap_lock.unlock();
        DEBUG(proc->is_debug,"return 0x%p",new_hint);
        return (std::int64_t)new_hint;
    } else {
        
        std::uint64_t mmap_base = 0;
        std::uint64_t mmap_size = 0;
        std::uint64_t mmap_flags = 0;
        userspace_fd_t* fd = vfs::fdmanager::search(proc,fd0);
        if(!fd) { mmap_lock.unlock();
            return -EBADF; }

        int status = vfs::vfs::mmap(fd,&mmap_base,&mmap_size,&mmap_flags);

        if(status == ENOSYS) {
            // try to fix this and read whole file 
            std::int64_t old_offset = fd->offset;
            fd->offset = off;

            vfs::stat_t stat;
            std::int32_t stat_status = vfs::vfs::stat(fd,&stat);

            if(!(stat.st_mode & S_IFREG)) { mmap_lock.unlock();
                return -EFAULT; }

            std::uint64_t new_hint_hint;

            if(!hint) {
                new_hint_hint = (std::uint64_t)memory::vmm::alloc(proc,size,PTE_PRESENT | PTE_USER | PTE_RW,0);
            } else {
                new_hint_hint = (std::uint64_t)memory::vmm::customalloc(proc,hint,size,PTE_PRESENT | PTE_USER | PTE_RW,0,0);
            }

            vfs::vfs::read(fd,Other::toVirt(memory::vmm::get(proc,new_hint_hint)->phys),size <= 0 ? stat.st_size : size);
            fd->offset = old_offset;

            mmap_lock.unlock();
            DEBUG(proc->is_debug,"returna 0x%p, is_fixed %d",new_hint_hint,flags & 0x100000);
            return (std::int64_t)new_hint_hint;

        }

        if(status != 0) { mmap_lock.unlock();
            return -status; }

        std::uint64_t new_hint_hint = (std::uint64_t)memory::vmm::map(proc,mmap_base,mmap_size,PTE_PRESENT | PTE_USER | PTE_RW | mmap_flags);
        memory::paging::enablepaging(ctx->cr3); // try to reset tlb
        
        mmap_lock.unlock();
        return (std::int64_t)new_hint_hint;
    }
}

long long sys_free(void *pointer, size_t size) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    memory::vmm::unmap(proc,(std::uint64_t)pointer);
    
    return 0;
}

long long sys_set_tid_address(int* tidptr) {
    SYSCALL_IS_SAFEZ((void*)tidptr,4096);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->tidptr = tidptr;
    
    return 0;
}

typedef struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} __attribute__((packed)) stackframe_t;

syscall_ret_t sys_fork(int D, int S, int d, int_frame_t* ctx) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* new_proc = arch::x86_64::scheduling::fork(proc,ctx);
    new_proc->ctx.rax = 0;
    new_proc->ctx.rdx = 0;

    arch::x86_64::scheduling::wakeup(new_proc);

    DEBUG(proc->is_debug,"Fork from proc %d, new proc %d",proc->id,new_proc->id);
    new_proc->is_debug = proc->is_debug;
    if(proc->is_debug) {
        stackframe_t* rbp = (stackframe_t*)ctx->rbp;
        Log::SerialDisplay(LEVEL_MESSAGE_INFO,"[0] - 0x%016llX (current rip)\n",ctx->rip);
        for (int i = 1; i < 10 && rbp; ++i) {
            std::uint64_t ret_addr = rbp->rip;
            Log::SerialDisplay(LEVEL_MESSAGE_INFO,"[%d] - 0x%016llX (0x%016llX)\n", i, ret_addr,ret_addr - memory::vmm::getlen(proc,ret_addr)->base);
            rbp = (stackframe_t*)rbp->rbp;
        }
    }

    return {1,0,new_proc->id};
}

/* Just give full io access to userspace */
syscall_ret_t sys_iopl(int a, int b ,int c , int_frame_t* ctx) {
    ctx->r11 |= (1 << 12) | (1 << 13);
    return {0,0,0};
}

syscall_ret_t sys_access_framebuffer(void* out) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    struct limine_framebuffer temp;
    memcpy(&temp,BootloaderInfo::AccessFramebuffer(),sizeof(struct limine_framebuffer));
    temp.address = (void*)((std::uint64_t)temp.address - BootloaderInfo::AccessHHDM());
    copy_in_userspace(proc,out,&temp,sizeof(struct limine_framebuffer));

    return {0,0,0};
}

std::uint64_t __elf_get_length2(char** arr) {
    std::uint64_t counter = 0;

    while(arr[counter])
        counter++;

    return counter;
}

long long sys_exec(char* path, char** argv, char** envp, int_frame_t* ctx) {

    if(!path || !argv || !envp)
        return -EINVAL;

    cpudata_t* cpdata = arch::x86_64::cpu::data();

    SYSCALL_IS_SAFEZ(path,4096);
    SYSCALL_IS_SAFEZ(argv,4096);
    SYSCALL_IS_SAFEZ(envp,4096);

    std::uint64_t argv_length = 0;
    std::uint64_t envp_length = 0;

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;

    argv_length = __elf_get_length2((char**)argv);
    envp_length = __elf_get_length2((char**)envp);

    char** argv0 = (char**)memory::heap::malloc(8 * (argv_length + 3));
    char** envp0 = (char**)memory::heap::malloc(8 * (envp_length + 1));

    memset(argv0,0,8 * (envp_length + 2));
    memset(envp0,0,8 * (envp_length + 1));

    char stack_path[2048];

    memset(stack_path,0,2048);

    for(int i = 0;i < argv_length; i++) {

        char* str = argv[i];

        char* new_str = (char*)memory::heap::malloc(strlen(str) + 1);

        memcpy(new_str,str,strlen(str));

        argv0[i] = new_str;

    }

    for(int i = 0;i < envp_length; i++) {

        char* str = envp[i];

        char* new_str = (char*)memory::heap::malloc(strlen(str) + 1);

        memcpy(new_str,str,strlen(str));

        envp0[i] = new_str;

    }

    copy_in_userspace_string(proc,stack_path,path,2048);

    char result[2048];
    vfs::resolve_path(stack_path,proc->cwd,result,1,0);

    vfs::stat_t stat;

    userspace_fd_t fd;
    fd.is_cached_path = 0;
    memset(fd.path,0,2048);
    memcpy(fd.path,result,strlen(result));

    int status = vfs::vfs::stat(&fd,&stat); 

    if(status != 0)
        return -status;

    vfs::fdmanager* fd_m = (vfs::fdmanager*)proc->fd;
    fd_m->cloexec();

    memset(proc->ret_handlers,0,sizeof(proc->ret_handlers));
    memset(proc->sig_handlers,0,sizeof(proc->sig_handlers));
    arch::x86_64::free_sigset_from_list(proc);

    DEBUG(proc->is_debug,"Exec file %s from proc %d",fd.path,proc->id);
    if(1) {
        for(int i = 0;i < argv_length;i++) {
            DEBUG(proc->is_debug,"Argv %d: %s",i,argv0[i]);
        }
    }

    if(status == 0) {
        if((stat.st_mode & S_IXUSR) && (stat.st_mode & S_IFREG)) {

            proc->fs_base = 0;

            memory::paging::enablekernel();
            memory::vmm::free(proc);

            memory::vmm::initproc(proc);

            memset(&proc->ctx,0,sizeof(int_frame_t));

            memory::vmm::reload(proc);
            
            proc->ctx.cs = 0x20 | 3;
            proc->ctx.ss = 0x18 | 3;

            proc->ctx.rflags = (1 << 9);

            status = arch::x86_64::scheduling::loadelf(proc,result,argv0,envp0,0);
            if(status == 0) {

                for(int i = 0;i < argv_length; i++) {
                    memory::heap::free(argv0[i]);
                }

                for(int i = 0;i < envp_length; i++) {
                    memory::heap::free(envp0[i]);
                }

                memory::heap::free(argv0);
                memory::heap::free(envp0);

                cpdata->temp.temp_ctx = 0;
                schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
                __builtin_unreachable();
            } 
            
            // maybe sh ?
            char interp[2048];
            memset(interp,0,2048);

            int i = 0;
            memcpy(interp,"/bin/sh",strlen("/bin/sh"));

            userspace_fd_t test;
            test.is_cached_path = 0;
            test.offset = 0;
            memset(test.path,0,2048);
            memcpy(test.path,result,strlen(result));

            

            char first;
            status = vfs::vfs::read(&test,&first,1);
            if(status > 0) {
                if(first == '!') {
                    memset(interp,0,2048);
                    while(1) {
                        int c = vfs::vfs::read(&test,&first,1);
                        if(c && c != '\n') 
                            interp[i++] = c;
                        if(c && c == '\n') 
                            break;
                    }
                } 
                argv_length++;
                memcpy(&argv0[2], argv0,sizeof(std::uint64_t) * argv_length);
                char* t1 = (char*)malloc(2048);
                memset(t1,0,2048);
                memcpy(t1,interp,strlen(interp));
                char* t2 = (char*)malloc(2048);
                memset(t2,0,2048);
                memcpy(t2,result,strlen(result));
                argv0[0] = t1;
                argv0[1] = t2;

                DEBUG(1,"interp %s to %s",argv0[0],argv0[1]);

                status = arch::x86_64::scheduling::loadelf(proc,interp,argv0,envp0,0);
                free((void*)t1);
                free((void*)t2);

                if(status == 0) {

                    for(int i = 0;i < argv_length; i++) {
                        memory::heap::free(argv0[i]);
                    }

                    for(int i = 0;i < envp_length; i++) {
                        memory::heap::free(envp0[i]);
                    }

                    memory::heap::free(argv0);
                    memory::heap::free(envp0);

                    cpdata->temp.temp_ctx = 0;
                    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
                    __builtin_unreachable();
                } 

            }

        }
    }



    for(int i = 0;i < argv_length; i++) {
        memory::heap::free(argv0[i]);
    }

    for(int i = 0;i < envp_length; i++) {
        memory::heap::free(envp0[i]);
    }

    memory::heap::free(argv0);
    memory::heap::free(envp0);

    proc->exit_code = -1;

    cpdata->temp.temp_ctx = 0;
    arch::x86_64::scheduling::kill(proc);
    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
    __builtin_unreachable();
}

long long sys_getpid() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return proc->id;
}

long long sys_getppid() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return proc->parent_id;
}

syscall_ret_t sys_gethostname(void* buffer, std::uint64_t bufsize) {

    if(!buffer)
        return {0,EINVAL,0};

    SYSCALL_IS_SAFEA(buffer,bufsize);

    const char* default_hostname = "orange-pc";
    arch::x86_64::process_t* proc = CURRENT_PROC;

    zero_in_userspace(proc,buffer,bufsize);
    copy_in_userspace(proc,buffer,(void*)default_hostname,strlen(default_hostname));

    return {0,0,0};
}

long long sys_getcwd(void* buffer, std::uint64_t bufsize) {

    SYSCALL_IS_SAFEZ(buffer,bufsize);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    zero_in_userspace(proc,buffer,bufsize);
    
    char buffer0[4096];
    memcpy(buffer0,proc->cwd,4096);

    copy_in_userspace(proc,buffer,buffer0,strlen((const char*)buffer0) > bufsize ? bufsize : strlen((const char*)buffer0));

    return 0;
}

long long sys_wait4(int pid,int* status,int flags) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* current = arch::x86_64::scheduling::head_proc_();

    DEBUG(proc->is_debug,"Trying to waitpid with pid %d from proc %d",pid,proc->id);
    
    if(pid < -1 || pid == 0)
        return -EINVAL;

    SYSCALL_IS_SAFEZ(status,4096);

    int success = 0;

    std::uint64_t current_timestamp = time::counter();
    std::uint64_t ns = 1000 * 1000 * 1000;

    if(pid == -1) {
        while (current)
        {
            if(current->parent_id == proc->id && (current->status = PROCESS_STATE_ZOMBIE || current->status == PROCESS_STATE_RUNNING) && current->waitpid_state != 2) {
                current->waitpid_state = 1;
                success = 1;
            }
            current = current->next;
        }
    } else if(pid > 0) {
        while (current)
        {
            if(current->parent_id == proc->id && (current->status = PROCESS_STATE_ZOMBIE || current->status == PROCESS_STATE_RUNNING) && current->id == pid && current->waitpid_state != 2) {
                current->waitpid_state = 1;
                success = 1;
                break;
            }
            current = current->next;
        }
    }

    if(!success) 
        return -ECHILD;

    int parent_id = proc->id;

    current = arch::x86_64::scheduling::head_proc_();
    while(1) {
        while (current)
        {
            if(current) {
                if(current->parent_id == parent_id && current->kill_lock.test() && current->waitpid_state == 1 && current->id != 0) {
                    current->waitpid_state = 2;
                    std::int64_t bro = (std::int64_t)(((std::uint64_t)current->exit_code) << 32) | current->id;
                    current->status = PROCESS_STATE_KILLED;
                    DEBUG(proc->is_debug,"Waitpid done pid %d from proc %d",pid,proc->id);
                    if(status)  
                        *status = current->exit_code;
                    return bro;
                }
            }
            current = current->next;
        }

        if(flags & WNOHANG) {
            arch::x86_64::process_t* pro = arch::x86_64::scheduling::head_proc_();
            while(pro) {
                if(pro->parent_id == parent_id && proc->waitpid_state == 1)
                    proc->waitpid_state = 0;
                pro = pro->next;
            }
            DEBUG(proc->is_debug,"Waitpid return WNOHAND from proc %d",proc->id);
            return 0;
        }
        signal_ret();
        yield();
        current = arch::x86_64::scheduling::head_proc_();
    }

}

syscall_ret_t sys_sleep(long us) {

    std::uint64_t current = drivers::tsc::currentnano();
    std::uint64_t end = us * 1000;
    while((drivers::tsc::currentnano() - current) < end) {
        if((drivers::tsc::currentnano() - current) < 15000) {
            asm volatile("pause");
        } else
            yield();
        signal_ret();
    }
    return {0,0,0};
}

syscall_ret_t sys_alloc_dma(std::uint64_t size) {
    return {1,0,memory::pmm::_physical::alloc(size)};
}

syscall_ret_t sys_free_dma(std::uint64_t phys) {
    memory::pmm::_physical::free(phys);
    return {0,0,0};
}

syscall_ret_t sys_map_phys(std::uint64_t phys, std::uint64_t flags, std::uint64_t size) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return {1,0,(std::int64_t)memory::vmm::map(proc,phys,size,PTE_PRESENT | PTE_USER | PTE_RW | flags)};
}

std::uint64_t timestamp = 0;

syscall_ret_t sys_timestamp() {
    timestamp = drivers::tsc::currentnano() > timestamp ? drivers::tsc::currentnano() : timestamp;
    return {1,0,(std::int64_t)timestamp};
}

syscall_ret_t sys_enabledebugmode() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->is_debug = !proc->is_debug;
    DEBUG(proc->is_debug,"Enabling/Disabling debug mode for proc %d",proc->id);
    return {0,0,0};
}

syscall_ret_t sys_enabledebugmodepid(int pid) {
    arch::x86_64::process_t* proc = arch::x86_64::scheduling::head_proc_();
    while(proc) {
        if(proc->id == pid)
            break;
        proc = proc->next;
    }
    if(!proc)
        return {0,ECHILD};

    DEBUG(proc->is_debug,"Enabling/Disabling debug mode for proc %d",proc->id);
    proc->is_debug = !proc->is_debug;
    return {0,0,0};
}

syscall_ret_t sys_printdebuginfo(int pid) {
    arch::x86_64::process_t* proc = arch::x86_64::scheduling::head_proc_();
    while(proc) {
        if(proc->id == pid)
            break;
        proc = proc->next;
    }
    if(!proc)
        return {0,ECHILD};

    DEBUG(1,"Process %d rip is 0x%p debug0 %d debug1 %d, sys %d",proc->id,proc->ctx.rip,proc->debug0,proc->debug1,proc->sys);
    stackframe_t* rbp = (stackframe_t*)proc->ctx.rbp;
        Log::SerialDisplay(LEVEL_MESSAGE_INFO,"[0] - 0x%016llX (current rip)\n",proc->ctx.rip);
        for (int i = 1; i < 10 && rbp; ++i) {
            std::uint64_t ret_addr = rbp->rip;
            Log::SerialDisplay(LEVEL_MESSAGE_INFO,"[%d] - 0x%016llX (0x%016llX)\n", i, ret_addr,0);
            rbp = (stackframe_t*)rbp->rbp;
        }
    return {0,0,0};
}


long long sys_clone3(clone_args* clargs, size_t size, int c, int_frame_t* ctx) {
    // size is ignored 
    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* new_proc = arch::x86_64::scheduling::clone3(proc,clargs,ctx);
    new_proc->ctx.rax = 0;

    arch::x86_64::scheduling::wakeup(new_proc);

    DEBUG(proc->is_debug,"clone3 from proc %d, new proc %d (new syscall_stack: 0x%p)",proc->id,new_proc->id,new_proc->syscall_stack);
    new_proc->is_debug = proc->is_debug;

    return new_proc->id;
}

// convert clone to clone3
long long sys_clone(unsigned long clone_flags, unsigned long newsp, int *parent_tidptr, int_frame_t* ctx) {
    std::uint64_t child_tidptr = ctx->r10;
    std::uint64_t tls = ctx->r8;
    clone_args arg = {0};
    arg.flags = clone_flags;
    arg.stack = newsp;
    arg.parent_tid = (std::uint64_t)parent_tidptr;
    arg.tls = tls;
    arg.child_tid = child_tidptr;
    return sys_clone3(&arg,sizeof(clone_args),0,ctx);
}

syscall_ret_t sys_breakpoint(int num, int b, int c, int_frame_t* ctx) {
    
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(proc->is_debug) {
        Log::SerialDisplay(LEVEL_MESSAGE_INFO,"breakpoint 0x%p\n",num);
        stackframe_t* rbp = (stackframe_t*)ctx->rbp;
        Log::SerialDisplay(LEVEL_MESSAGE_INFO,"[0] - 0x%016llX (current rip)\n",ctx->rip);
        for (int i = 1; i < 10 && rbp; ++i) {
            std::uint64_t ret_addr = rbp->rip;
            Log::SerialDisplay(LEVEL_MESSAGE_INFO,"[%d] - 0x%016llX (0x%016llX)\n", i, ret_addr,ret_addr - memory::vmm::getlen(proc,ret_addr)->base);
            rbp = (stackframe_t*)rbp->rbp;
        }
    }
    return {0,0,0};
}

syscall_ret_t sys_copymemory(void* src, void* dest, int len) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    copy_in_userspace(proc,dest,src,len);
    return {0,0,0};
}

#define PRIO_PROCESS 1
#define PRIO_PGRP 2
#define PRIO_USER 3

long long sys_setpriority(int which, int who, int prio) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(which == PRIO_PROCESS) { 
        
        arch::x86_64::process_t* need_proc = arch::x86_64::scheduling::head_proc_();
        while(need_proc) {
            if(need_proc->id == who) 
                break;
            need_proc = need_proc->next;
        }

        if(!need_proc)
            return -ESRCH;

        DEBUG(proc->is_debug,"Setpriority %d to proc %d (who %d) which %d from proc %d",prio,need_proc->id,who,which,proc->id);
        need_proc->prio = prio;
        
    } else
        return -ENOSYS;
    return 0;
}

long long sys_getpriority(int which, int who) {
    int prio = 0;
    if(which == 0) { // PRIO_PROCESS
        arch::x86_64::process_t* proc = CURRENT_PROC;
        
        arch::x86_64::process_t* need_proc = arch::x86_64::scheduling::head_proc_();
        while(need_proc) {
            if(need_proc->id == who) 
                break;
            need_proc = need_proc->next;
        }

        if(!need_proc)
            return -ESRCH;
        prio = need_proc->prio;
        DEBUG(proc->is_debug,"Getpriority %d to proc %d (who %d) from proc %d",prio,need_proc->id,who,proc->id);
    } else
        return -ENOSYS;
    return prio;
}

syscall_ret_t sys_yield() {
    yield();
    return {0,0,0};
}

syscall_ret_t sys_dmesg(char* buf,std::uint64_t count) {
    SYSCALL_IS_SAFEA(buf,count);
    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(!buf) {
        return {1,0,(std::int64_t)dmesg_bufsize()};
    }

    vmm_obj_t* vmm_object = memory::vmm::getlen(proc,(std::uint64_t)buf);
    uint64_t need_phys = vmm_object->phys + ((std::uint64_t)buf - vmm_object->base);

    std::uint64_t offset_start = (std::uint64_t)buf - vmm_object->base;
    std::uint64_t end = vmm_object->base + vmm_object->len;

    char* temp_buffer = (char*)Other::toVirt(need_phys);
    memset(temp_buffer,0,count);

    dmesg_read(temp_buffer,count);

    return {1,0,0};
}

long long sys_getuid() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return proc->uid;
}

long long sys_getpgrp() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return proc->thread_group;
}

long long sys_setpgid(int pid, int pgid) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(pid > 0) {
        proc = arch::x86_64::scheduling::by_pid(pid);
        if(!proc)
            return -ESRCH;
    }
    proc->thread_group = pgid;
    return 0;
}

long long sys_getpgid(int pid) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(pid > 0) {
        proc = arch::x86_64::scheduling::by_pid(pid);
        if(!proc)
            return -ESRCH;
    }
    return proc->thread_group;
}

long long sys_getresuid(int* ruid, int* euid, int *suid) {

    SYSCALL_IS_SAFEZ(ruid,4096);
    SYSCALL_IS_SAFEZ(euid,4096);
    SYSCALL_IS_SAFEZ(suid,4096);

    if(!ruid || !euid || !suid)
        return -EINVAL;

    *ruid = sys_getuid();
    *euid = sys_getuid();
    *suid = sys_getuid();
    return 0;
}

long long sys_setuid(int uid) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(proc->uid > 1000)
        return -EPERM;
    proc->uid = uid;
    return 0;
}

// used from mlibc source, why not 

#define CHAR_BIT 8
#define CPU_MASK_BITS (CHAR_BIT * sizeof(__cpu_mask))

std::uint64_t __mlibc_cpu_alloc_size(int num_cpus) {
	/* calculate the (unaligned) remainder that doesn't neatly fit in one __cpu_mask; 0 or 1 */
	std::uint64_t remainder = ((num_cpus % CPU_MASK_BITS) + CPU_MASK_BITS - 1) / CPU_MASK_BITS;
	return sizeof(__cpu_mask) * (num_cpus / CPU_MASK_BITS + remainder);
}

#define CPU_ALLOC_SIZE(n) __mlibc_cpu_alloc_size((n))

void __mlibc_cpu_zero(const std::uint64_t setsize, cpu_set_t *set) {
	memset(set, 0, CPU_ALLOC_SIZE(setsize));
}

void __mlibc_cpu_set(const int cpu, const std::uint64_t setsize, cpu_set_t *set) {
	if(cpu >= static_cast<int>(setsize * CHAR_BIT)) {
		return;
	}

	unsigned char *ptr = reinterpret_cast<unsigned char *>(set);
	std::uint64_t off = cpu / CHAR_BIT;
	std::uint64_t mask = 1 << (cpu % CHAR_BIT);

	ptr[off] |= mask;
}

#define CPU_ZERO_S(setsize, set) __mlibc_cpu_zero((setsize), (set))
#define CPU_ZERO(set) CPU_ZERO_S(sizeof(cpu_set_t), set)
#define CPU_SET_S(cpu, setsize, set) __mlibc_cpu_set((cpu), (setsize), (set))
#define CPU_SET(cpu, set) CPU_SET_S(cpu, sizeof(cpu_set_t), set)

void __fill_cpu_set(cpu_set_t* cpuset) {

    extern int how_much_cpus;
    
    CPU_ZERO(cpuset);
    
    for(int i = 0; i < how_much_cpus; i++) {
        CPU_SET(i, cpuset); 
    }
}

syscall_ret_t sys_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {

    arch::x86_64::process_t* proc = 0;

    if(cpusetsize > sizeof(cpu_set_t))
        cpusetsize = sizeof(cpu_set_t);

    SYSCALL_IS_SAFEA(mask,cpusetsize);
    if(!mask)
        return {0,EINVAL,0};

    if(pid == 0) {
        proc = CURRENT_PROC;
    } else if(pid > 0) {
        proc = arch::x86_64::scheduling::by_pid(pid);
        if(!proc)
            return {0,ESRCH,0};
    } else 
        return {0,EINVAL,0};

    cpu_set_t temp_set;
    memset(&temp_set,0,sizeof(cpu_set_t));

    __fill_cpu_set(&temp_set);

    memset(mask,0,cpusetsize);
    memcpy(mask,&temp_set,cpusetsize);

    return {0,0,0};
}

syscall_ret_t sys_cpucount() {
    extern int how_much_cpus;
    return {0,how_much_cpus,0};
}

long long sys_brk() {
    return 0;
}

#define ARCH_SET_GS                     0x1001
#define ARCH_SET_FS                     0x1002
#define ARCH_GET_FS                     0x1003
#define ARCH_GET_GS                     0x1004

#define ARCH_GET_CPUID                  0x1011
#define ARCH_SET_CPUID                  0x1012

long long sys_arch_prctl(int option, unsigned long* addr) {
    if(!addr)
        return -EINVAL;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    switch(option) {
    case ARCH_SET_FS: 
        proc->fs_base = (std::uint64_t)addr;
        __wrmsr(0xC0000100,(std::uint64_t)addr);
        return 0;
    case ARCH_GET_FS:
        SYSCALL_IS_SAFEZ(addr,4096);
        *addr = proc->fs_base;
        return 0;
    default:
        Log::SerialDisplay(LEVEL_MESSAGE_FAIL,"unsupported arch_prctl option %p, addr 0x%p, shit %d\n",option,addr,0);
        return -EINVAL;
    };

    return 0;
}