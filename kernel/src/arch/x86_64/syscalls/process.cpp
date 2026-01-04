
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
    DEBUG(1,"%s from proc %d",buffer,proc->id);

    return {0,0,0};
}

syscall_ret_t sys_exit(int status) {

    memory::paging::enablekernel();

    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->exit_code = status;

    DEBUG(proc->is_debug,"Process %s (%d) exited with code %d",proc->name,proc->id,status);

    arch::x86_64::scheduling::kill(proc);
    
    if(1)
        memory::vmm::free(proc); 

    vfs::fdmanager* fd = (vfs::fdmanager*)proc->fd;
    fd->free();

    memory::pmm::_virtual::free(proc->cwd);
    memory::pmm::_virtual::free(proc->name);
    memory::pmm::_virtual::free(proc->sse_ctx);
    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
    __builtin_unreachable();
}

locks::spinlock mmap_lock; // memory access should be locked anyway 

syscall_ret_t sys_mmap(std::uint64_t hint, std::uint64_t size, int fd0, int_frame_t* ctx) {

    SYSCALL_DISABLE_PREEMPT(); // ensure interrupts disabled
    mmap_lock.lock();

    std::uint64_t flags = ctx->r8;
    arch::x86_64::process_t* proc = CURRENT_PROC;
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
            memory::vmm::customalloc(proc,new_hint,size,PTE_PRESENT | PTE_RW | PTE_USER,0);

        memory::paging::enablepaging(ctx->cr3); // try to reset tlb

        mmap_lock.unlock();
        return {1,0,(std::int64_t)new_hint};
    } else {
        
        std::uint64_t mmap_base = 0;
        std::uint64_t mmap_size = 0;
        std::uint64_t mmap_flags = 0;
        userspace_fd_t* fd = vfs::fdmanager::search(proc,fd0);

        DEBUG(proc->is_debug,"Trying to mmap fd %d from proc %d",fd0,proc->id);
        if(!fd) { mmap_lock.unlock();
            return {1,EBADF,0}; }
        DEBUG(proc->is_debug,"Trying to mmap %s from proc %d",fd->path,proc->id);

        int status = vfs::vfs::mmap(fd,&mmap_base,&mmap_size,&mmap_flags);

        if(status == ENOSYS) {
            // try to fix this and read whole file 
            std::int64_t old_offset = fd->offset;
            fd->offset = 0;

            vfs::stat_t stat;
            std::int32_t stat_status = vfs::vfs::stat(fd,&stat);

            if(!(stat.st_mode & S_IFREG)) { mmap_lock.unlock();
                return {1,EFAULT,0}; }

            std::uint64_t new_hint_hint = (std::uint64_t)memory::vmm::alloc(proc,size,PTE_PRESENT | PTE_USER | PTE_RW,0);

            vfs::vfs::read(fd,Other::toVirt(memory::vmm::get(proc,new_hint_hint)->phys),size <= 0 ? stat.st_size : size);
            fd->offset = old_offset;

            mmap_lock.unlock();
            return {1,0,(std::int64_t)new_hint_hint};

        }

        if(status != 0) { mmap_lock.unlock();
            return {1,status,0}; }

        std::uint64_t new_hint_hint = (std::uint64_t)memory::vmm::map(proc,mmap_base,mmap_size,PTE_PRESENT | PTE_USER | PTE_RW | mmap_flags);
        memory::paging::enablepaging(ctx->cr3); // try to reset tlb
        
        mmap_lock.unlock();
        return {1,0,(std::int64_t)new_hint_hint};
    }
}

syscall_ret_t sys_free(void *pointer, size_t size) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    memory::vmm::unmap(proc,(std::uint64_t)pointer);
    
    return {0,0,0};
}

syscall_ret_t sys_fork(int D, int S, int d, int_frame_t* ctx) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* new_proc = arch::x86_64::scheduling::fork(proc,ctx);
    new_proc->ctx.rax = 0;
    new_proc->ctx.rdx = 0;

    arch::x86_64::scheduling::wakeup(new_proc);

    DEBUG(0,"Fork from proc %d, new proc %d",proc->id,new_proc->id);
    new_proc->is_debug = proc->is_debug;

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

syscall_ret_t sys_exec(char* path, char** argv, char** envp, int_frame_t* ctx) {

    if(!path || !argv || !envp)
        return {0,EINVAL,0};

    SYSCALL_IS_SAFEA(path,4096);
    SYSCALL_IS_SAFEA(argv,4096);
    SYSCALL_IS_SAFEA(envp,4096);

    std::uint64_t argv_length = 0;
    std::uint64_t envp_length = 0;

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;

    argv_length = __elf_get_length2((char**)argv);
    envp_length = __elf_get_length2((char**)envp);

    char** argv0 = (char**)memory::heap::malloc(8 * (argv_length + 2));
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

    DEBUG(proc->is_debug,"Exec file %s from proc %d",fd.path,proc->id);
    if(proc->is_debug) {
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
                memcpy(&argv0[1], argv0,sizeof(std::uint64_t) * argv_length);
                char* t1 = (char*)malloc(2048);
                memset(t1,0,2048);
                memcpy(t1,result,strlen(result));
                argv0[0] = t1;
                

                status = arch::x86_64::scheduling::loadelf(proc,interp,argv0,envp0,0);
                if(status == 0) {

                    for(int i = 0;i < argv_length; i++) {
                        memory::heap::free(argv0[i]);
                    }

                    for(int i = 0;i < envp_length; i++) {
                        memory::heap::free(envp0[i]);
                    }

                    memory::heap::free(argv0);
                    memory::heap::free(envp0);

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

    arch::x86_64::scheduling::kill(proc);
    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
    __builtin_unreachable();
}

syscall_ret_t sys_getpid() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return {0,(int)proc->id,0};
}

syscall_ret_t sys_getppid() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return {0,(int)proc->parent_id,0};
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

syscall_ret_t sys_getcwd(void* buffer, std::uint64_t bufsize) {

    SYSCALL_IS_SAFEA(buffer,bufsize);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    zero_in_userspace(proc,buffer,bufsize);
    
    char buffer0[4096];
    memcpy(buffer0,proc->cwd,4096);

    copy_in_userspace(proc,buffer,buffer0,strlen((const char*)buffer0) > bufsize ? bufsize : strlen((const char*)buffer0));

    return {0,0,0};
}

syscall_ret_t sys_waitpid(int pid,int flags) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* current = arch::x86_64::scheduling::head_proc_();

    DEBUG(proc->is_debug,"Trying to waitpid with pid %d from proc %d",pid,proc->id);
    
    if(pid < -1 || pid == 0)
        return {0,ENOSYS,0};

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
        return {0,ECHILD,0};

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
                    return {1,0,bro};
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
            return {1,0,0};
        }
        yield();
        current = arch::x86_64::scheduling::head_proc_();
    }

}

syscall_ret_t sys_sleep(long us) {

    int enable_optimization = 0;

    if(!enable_optimization)
        SYSCALL_ENABLE_PREEMPT();

    std::uint64_t current = drivers::tsc::currentnano();
    std::uint64_t end = us * 1000;
    while((drivers::tsc::currentnano() - current) < end);
        if(enable_optimization)
            yield();
        else
            asm volatile("pause");
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

    DEBUG(1,"Process %d rip is 0x%p debug0 %d debug1 %d",proc->id,proc->ctx.rip,proc->debug0,proc->debug1);
    return {0,0,0};
}

syscall_ret_t sys_clone(std::uint64_t stack, std::uint64_t rip, int c, int_frame_t* ctx) {
    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* new_proc = arch::x86_64::scheduling::clone(proc,ctx);

    vmm_obj_t* st = (vmm_obj_t*)new_proc->vmm_start;

    st->lock.lock();
    st->pthread_count++;
    st->lock.unlock();

    new_proc->ctx.rax = 0;
    new_proc->ctx.rdx = 0;

    //new_proc->fs_base = proc->fs_base;
    
    new_proc->ctx.rsp = stack;
    new_proc->ctx.rip = rip;

    arch::x86_64::scheduling::wakeup(new_proc);

    DEBUG(proc->is_debug,"Clone from proc %d, new proc %d (new syscall_stack: 0x%p)",proc->id,new_proc->id,new_proc->syscall_stack);
    new_proc->is_debug = proc->is_debug;

    return {1,0,new_proc->id};
}

syscall_ret_t sys_breakpoint(int num) {
    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"breakpoint %d\n",num);
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

syscall_ret_t sys_setpriority(int which, int who, int prio) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(which == PRIO_PROCESS) { 
        
        arch::x86_64::process_t* need_proc = arch::x86_64::scheduling::head_proc_();
        while(need_proc) {
            if(need_proc->id == who) 
                break;
            need_proc = need_proc->next;
        }

        if(!need_proc)
            return {0,ESRCH,0};

        DEBUG(proc->is_debug,"Setpriority %d to proc %d (who %d) which %d from proc %d",prio,need_proc->id,who,which,proc->id);
        need_proc->prio = prio;
        
    } else
        return {0,ENOSYS,0};
    return {0,0,0};
}

syscall_ret_t sys_getpriority(int which, int who) {
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
            return {1,ESRCH,0};

        prio = need_proc->prio;
        DEBUG(proc->is_debug,"Getpriority %d to proc %d (who %d) from proc %d",prio,need_proc->id,who,proc->id);
    } else
        return {1,ENOSYS,0};
    return {1,0,prio};
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

syscall_ret_t sys_getuid() {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    return {0,proc->uid,0};
}

syscall_ret_t sys_setuid(int uid) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(proc->uid > 1000)
        return {0,EPERM,0};
    proc->uid = uid;
    return {0,0,0};
}

syscall_ret_t sys_kill(int pid, int sig) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    arch::x86_64::process_t* target_proc = arch::x86_64::scheduling::by_pid(pid);

    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"sys_kill pid %d sig %d from proc %d\n",pid,sig,proc->id);

    if(!target_proc)
        return {0,ESRCH,0};

    if(!(proc->uid == 0 || proc->uid == target_proc->uid))
        return {0,EPERM,0};

    switch(sig) {
        case SIGKILL:
            target_proc->exit_code = 137;
            target_proc->_3rd_kill_lock.nowaitlock(); 
            break;
        default:
            return {0,0,0};
    }
    return {0,0,0};
}