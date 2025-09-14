
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/vmm.hpp>

#include <generic/vfs/fd.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>

#include <etc/errno.hpp>

#include <generic/time.hpp>

#include <generic/vfs/vfs.hpp>

#include <etc/bootloaderinfo.hpp>

syscall_ret_t sys_tcb_set(std::uint64_t fs) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    __wrmsr(0xC0000100,fs);
    return {0,0,0};
}

syscall_ret_t sys_libc_log(const char* msg) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    char buffer[2048];
    memset(buffer,0,2048);
    copy_in_userspace_string(proc,buffer,(void*)msg,2048);
    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"libc_log from proc %d: %s\n",proc->id,buffer);
    return {0,0,0};
}

syscall_ret_t sys_exit(int status) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->exit_code = status;
    arch::x86_64::scheduling::kill(proc);
    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"Process %d exited with code %d\n",proc->id,proc->exit_code);
    schedulingScheduleAndChangeStack(arch::x86_64::cpu::data()->timer_ist_stack,0);
    __builtin_unreachable();
}

syscall_ret_t sys_mmap(std::uint64_t hint, std::uint64_t size, int fd0, int_frame_t* ctx) {
    std::uint64_t flags = ctx->r8;
    arch::x86_64::process_t* proc = CURRENT_PROC;
    if(flags & MAP_ANONYMOUS) {
        std::uint64_t new_hint = hint;
        if(!new_hint)
            new_hint = (std::uint64_t)memory::vmm::alloc(proc,size,PTE_PRESENT | PTE_USER | PTE_RW);
        else 
            memory::vmm::customalloc(proc,new_hint,size,PTE_PRESENT | PTE_RW | PTE_USER);

        return {1,0,(std::int64_t)new_hint};
    } else {
        
        std::uint64_t mmap_base = 0;
        std::uint64_t mmap_size = 0;
        std::uint64_t mmap_flags = 0;
        userspace_fd_t* fd = vfs::fdmanager::search(proc,fd0);
        if(!fd)
            return {1,EBADF,0};
        int status = vfs::vfs::mmap(fd,&mmap_base,&mmap_size,&mmap_flags);
        if(status != 0)
            return {1,status,0};

        std::uint64_t new_hint_hint = (std::uint64_t)memory::vmm::map(proc,mmap_base,mmap_size,PTE_PRESENT | PTE_USER | PTE_RW | mmap_flags);
        return {1,0,(std::int64_t)new_hint_hint};
    }
}

syscall_ret_t sys_free(void *pointer, size_t size) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    std::uint64_t phys = memory::vmm::get(proc,(std::uint64_t)pointer)->phys;
    if(!memory::vmm::get(proc,(std::uint64_t)pointer)->is_mapped)
        memory::pmm::_physical::free(phys);
    memory::vmm::get(proc,(std::uint64_t)pointer)->phys = 0;
    return {0,0,0};
}

syscall_ret_t sys_fork(int D, int S, int d, int_frame_t* ctx) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* new_proc = arch::x86_64::scheduling::fork(proc,ctx);
    new_proc->ctx.rax = 0;
    new_proc->ctx.rdx = 0;
    
    arch::x86_64::scheduling::wakeup(new_proc);


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

    vfs::vfs::lock();
    memory::pmm::_physical::lock();
    memory::heap::lock();

    __cli();

    arch::x86_64::process_t* proc = arch::x86_64::cpu::data()->temp.proc;

    memory::heap::unlock();
    memory::pmm::_physical::unlock();

    memory::paging::enablepaging(ctx->cr3);
    argv_length = __elf_get_length2((char**)argv);
    envp_length = __elf_get_length2((char**)envp);
    memory::paging::enablekernel();

    char** argv0 = (char**)memory::heap::malloc(8 * (argv_length + 1));
    char** envp0 = (char**)memory::heap::malloc(8 * (envp_length + 1));

    memset(argv0,0,8 * (envp_length + 1));
    memset(envp0,0,8 * (envp_length + 1));

    char stack_path[2048];

    memset(stack_path,0,2048);

    memory::paging::enablepaging(ctx->cr3);

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

    memory::paging::enablekernel();

    copy_in_userspace_string(proc,stack_path,path,2048);

    char result[2048];
    vfs::resolve_path(stack_path,proc->cwd,result,1,0);

    memory::vmm::free(proc);
    memory::vmm::initproc(proc);

    vfs::stat_t stat;

    userspace_fd_t fd;
    memset(fd.path,0,2048);
    memcpy(fd.path,stack_path,strlen(stack_path));

    int status = vfs::vfs::nlstat(&fd,&stat); 
    
    vfs::vfs::unlock();

    if(status == 0) {
        if((stat.st_mode & S_IXUSR) && (stat.st_mode & S_IFCHR)) {

            proc->fs_base = 0;

            memset(&proc->ctx,0,sizeof(int_frame_t));
            
            proc->ctx.cs = 0x20 | 3;
            proc->ctx.ss = 0x18 | 3;

            proc->ctx.rflags = (1 << 9);
        
            memory::vmm::reload(proc);

            arch::x86_64::scheduling::loadelf(proc,stack_path,argv0,envp0);


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

    for(int i = 0;i < argv_length; i++) {
        memory::heap::free(argv0[i]);
    }

    for(int i = 0;i < envp_length; i++) {
        memory::heap::free(envp0[i]);
    }

    memory::heap::free(argv0);
    memory::heap::free(envp0);

    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"exec error\n");
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

syscall_ret_t sys_waitpid(int pid) {
    extern arch::x86_64::process_t* head_proc;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    arch::x86_64::process_t* current = head_proc;
    
    if(pid < -1 || pid == 0)
        return {0,ENOSYS,0};

    int success = 0;

    std::uint64_t current_timestamp = time::counter();
    std::uint64_t ns = 1000 * 1000 * 1000;

    if(pid == -1) {
        while (current)
        {
            if(current->parent_id == proc->id && (current->status = PROCESS_STATE_ZOMBIE || current->status == PROCESS_STATE_RUNNING) &&current->waitpid_state == 0) {
                current->waitpid_state = 1;
                success = 1;
            }
            current = current->next;
        }
    } else if(pid > 0) {
        while (current)
        {
            if(current->parent_id == proc->id && (current->status = PROCESS_STATE_ZOMBIE || current->status == PROCESS_STATE_RUNNING) && current->id == pid) {
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

    SYSCALL_ENABLE_PREEMPT();

    current = head_proc;
    while(1) {
        while (current)
        {
            if(current) {
                if(current->parent_id == parent_id && current->kill_lock.test() && current->waitpid_state == 1 && current->id != 0) {
                    current->waitpid_state = 2;
                    std::int64_t bro = (std::int64_t)(((std::uint64_t)current->exit_code) << 32) | current->id;
                    current->status = PROCESS_STATE_KILLED;
                    return {1,0,bro};
                }
            }
            current = current->next;
        }
        current = head_proc;
    }

}

syscall_ret_t sys_sleep(long us) {
    SYSCALL_ENABLE_PREEMPT();
    time::sleep(us);
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