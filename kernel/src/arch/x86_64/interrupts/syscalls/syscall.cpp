
#include <arch/x86_64/interrupts/syscalls/syscall.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>
#include <other/log.hpp>
#include <stdint.h>
#include <other/assembly.hpp>
#include <drivers/hpet/hpet.hpp>
#include <generic/memory/paging.hpp>
#include <generic/memory/pmm.hpp>
#include <generic/memory/heap.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/VFS/tmpfs.hpp>
#include <config.hpp>
#include <other/hhdm.hpp>
#include <other/string.hpp>
#include <arch/x86_64/interrupts/syscalls/ipc/fd.hpp>
#include <other/other.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <generic/memory/vmm.hpp>
#include <generic/VFS/ustar.hpp>
#include <other/assert.hpp>
#include <other/other.hpp>
#include <other/debug.hpp>

extern "C" void syscall_handler();

// syscalls

int syscall_exit(int_frame_t* ctx) {

    process_t* proc = CpuData::Access()->current;
    
    Process::Kill(proc,ctx->rdi);

    //Log("Process %d is died with code %d !\n",proc->id,proc->return_status);

    schedulingSchedule(ctx);
    
}

int syscall_debug_print(int_frame_t* ctx) {

    uint64_t size = ctx->rsi;
    char buffer[1024];
    
    String::memset(buffer,0,1024);

    char* ptr = &buffer[0];

    String::memset(ptr,0,size);

    char* src = (char*)ctx->rdi;

    process_t* proc = CpuData::Access()->current;

    if(ctx->cr3) { 

        Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
        String::memcpy(ptr,src,size);
        Paging::EnableKernel();

#ifdef DEBUG_PRINT
        Log(LOG_LEVEL_DEBUG,"%s\n",ptr);
#endif

        return 0;

    } else {
        return -2;
    }

    return 0;

}

int syscall_futex_wait(int_frame_t* ctx) {

    int* p = (int*)ctx->rdi;
    int e = ctx->rsi;

    process_t* proc = CpuData::Access()->current;
    Process::futexWait(proc,p,e);

    return 0;

}

int syscall_futex_wake(int_frame_t* ctx) {

    int *p = (int*)ctx->rdi;

    process_t* proc = CpuData::Access()->current;

    if(proc->parent_process)
        proc = Process::ByID(proc->parent_process);

    Process::futexWake(proc,p);

    return 0;

}

int syscall_tcb_set(int_frame_t* ctx) {
    
    uint64_t fs_base = ctx->rdi;
    process_t* proc = CpuData::Access()->current;

    __wrmsr(0xC0000100,fs_base);
    proc->fs_base = fs_base;

    return 0;

}

int syscall_open(int_frame_t* ctx) {

    char* name = (char*)ctx->rdi;
    int* fdout = (int*)ctx->rsi;

    process_t* proc = CpuData::Access()->current;

    if(!name && !fdout)
        return -2;

    char buffer[4096];

    String::memset(buffer,0,4096);

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(buffer,name,String::strlen(name));
    Paging::EnableKernel();    

    if(buffer[String::strlen(buffer) - 1] == '/')
        buffer[String::strlen(buffer) - 1] = '\0';

    char* first = proc->cwd;
    if(!first)
        first = "/";

    char buf[1024];
    char* path = (char*)buf;
    String::memset(buf,0,1024);

    int ptr = String::strlen(first);
    String::memcpy(buf,first,ptr);

    buf[ptr] = '/';
    buf[ptr + 1] = 'd';

    resolve_path(buffer,first,path);

    int fd = FD::Create(proc,0);
    fd_t* fd_s = FD::Search(proc,fd);
    String::memset(fd_s->path_point,0,2048);
    String::memcpy(fd_s->path_point,path,String::strlen(path));

    if(!fd_s)
        return -3;

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    *fdout = fd;
    Paging::EnableKernel();

    //Log(LOG_LEVEL_DEBUG,"Touching %s\n",path);

    VFS::Touch(path);

    return 0;

}

// SEEK_SET 0 start
// SEEK_CUR 1 current
// SEEK_END 2 end

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int syscall_seek(int_frame_t* ctx) {

    int fd = ctx->rdi;
    long offset = (long)ctx->rsi;
    int whence = ctx->rdx;

    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);

    if(!file)
        return -1;

    filestat_t stat;
    int stx = VFS::Stat(file->path_point,(char*)&stat); 

    switch (whence)
    {
        case SEEK_SET:
            file->seek_offset = offset;
            break;

        case SEEK_CUR:
            file->seek_offset += offset;
            break;

        case SEEK_END:
            file->seek_offset = stx == 0 ? stat.size + offset : offset;
            break;

        default:
            Log(LOG_LEVEL_DEBUG,"Process %d, sys_seek, unhandled whence: %d.\n",CpuData::Access()->current->id,whence);
            return 22;

    }

    ctx->rdx = file->seek_offset;

    return 0;

}

/*
[[gnu::weak]] int sys_tcgetattr(int fd, struct termios *attr);
[[gnu::weak]] int sys_tcsetattr(int, int, const struct termios *attr);
*/

int syscall_tcgetattr(int_frame_t* ctx) {

    

}

int syscall_tcsetattr(int_frame_t* ctx) {

}

void __prepare_file_content_syscall(char* content,uint64_t size,uint64_t phys_cr3) {

    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(phys_cr3);
    uint64_t phys_file = HHDM::toPhys((uint64_t)content);
    uint64_t aligned_size_in_pages = ALIGNPAGEUP(size) / PAGE_SIZE;
    for(uint64_t i = 0;i < aligned_size_in_pages;i++) {
        Paging::HHDMMap(cr3,phys_file + (i * PAGE_SIZE),PTE_PRESENT | PTE_RW);
    }


}

extern "C" void syscall_end(int_frame_t* ctx);

extern "C" int syscall_read_stage_2(int_frame_t* ctx,fd_t* file) {

    void* buf = (void*)ctx->rsi;
    uint64_t count = ctx->rdx;

    if(file->path_point[0])
        VFS::AskForPipe(file->path_point,&file->pipe);

    process_t* proc = CpuData::Access()->current;

    while(1) {
        if(!file->pipe.is_received) {

            if(file->pipe.buffer_size < count)
                count = file->pipe.buffer_size;

            char* pipe_buffer = file->pipe.buffer;

            __prepare_file_content_syscall(file->pipe.buffer,count,ctx->cr3);
            Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
            String::memcpy(buf,pipe_buffer,count);
            Paging::EnableKernel();
            file->pipe.is_received = 1;
            ctx->rdx = 1;
            ctx->rax = 0;

            proc->is_eoi = 1;

            Lapic::EOI();
            syscall_end(ctx);

        } else {
            asm volatile("int $32");
        }
    }

}

extern "C" void syscall_read_stage_2_asm(uint64_t new_stack,int_frame_t* ctx,fd_t* file);

int syscall_read(int_frame_t* ctx) {

    int fd = ctx->rdi;
    void* buf = (void*)ctx->rsi;
    uint64_t count = ctx->rdx;
    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);
    filestat_t stat;

    if(!file)
        return -1;

    //Log(LOG_LEVEL_DEBUG,"%s\n",file->path_point);

    if(file->type == FD_PIPE) {
        
        //proc->is_cli = 0;
        if(file->pipe.type == PIPE_WAIT) {
            proc->is_eoi = 0;
            String::memcpy(proc->syscall_wait_ctx,ctx,sizeof(int_frame_t));
            syscall_read_stage_2_asm((uint64_t)proc->wait_stack + 4096,proc->syscall_wait_ctx,file);
            return -20;
        } else if(file->pipe.type == PIPE_INSTANT) {
            
            if(!file->pipe.is_used) 
                VFS::AskForPipe(file->path_point,&file->pipe);

            if(!file->pipe.is_received) {

                if(file->pipe.buffer_size < count)
                    count = file->pipe.buffer_size;
        
                char* pipe_buffer = file->pipe.buffer;
        
                __prepare_file_content_syscall(file->pipe.buffer,count,ctx->cr3);

                Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
                String::memcpy(buf,pipe_buffer,count);
                Paging::EnableKernel();
                file->pipe.is_received = 1;
                ctx->rdx = 1;
        
                proc->is_eoi = 1;

                return 0;

        
            } else {
                ctx->rdx = 0;
                return 0;
            }

        }
        
        
    
    }

    if(VFS::Stat(file->path_point,(char*)&stat) == -15) {
        
        char* dest_buf = (char*)PMM::VirtualBigAlloc(SIZE_TO_PAGES(count));

        __prepare_file_content_syscall(dest_buf,count,ctx->cr3);

        VFS::Read(file->path_point,file->path_point,count);

        Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
        String::memset(buf,0,count);
        String::memcpy(buf,dest_buf,count);
        Paging::EnableKernel();

        ctx->rdx = count;
        
        return 0;
    }

    long seek_off = file->seek_offset;

    uint64_t actual_size = 0;
    char is_file_bigger = 0;

    if(count < stat.size + seek_off) {
        actual_size = count;
        is_file_bigger = 1;
    } else if(count > stat.size + seek_off) {
        actual_size = stat.size;
        is_file_bigger = 0;
    } else {
        actual_size = count;
        is_file_bigger = 1;
    }

    char* actual_src = stat.content;

    if(seek_off > 0)
        actual_src = (char*)((uint64_t)actual_src + seek_off);
    else if(seek_off < 0){
        actual_size += seek_off;
        
    }

    if(stat.type == VFS_TYPE_DIRECTORY)
        return 21;

    if(stat.content) {
        __prepare_file_content_syscall(stat.content,stat.size,ctx->cr3);
    } else {
        actual_src = "";
        actual_size = 0;
    }

    file->seek_offset += actual_size;

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memset(buf,0,count);
    if(!is_file_bigger) {
        String::memcpy(buf,actual_src,actual_size);
    } else {
        String::memcpy(buf,actual_src,count);
    }
    Paging::EnableKernel();

    ctx->rdx = actual_size;

    return 0;

}

int syscall_write(int_frame_t* ctx) {

    int fd = ctx->rdi;
    void* buf = (void*)ctx->rsi;
    uint64_t count = ctx->rdx;

    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);
    filestat_t stat;

    if(!file)
        return -1;

    if(file->type == FD_PIPE) {
        
        if(count > (1024 * 64))
            count = (1024 * 64) - 1;

        char* pipe_buffer = file->pipe.buffer;
        file->pipe.buffer_size = count;

        __prepare_file_content_syscall(pipe_buffer,count,ctx->cr3);

        Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
        String::memcpy(pipe_buffer,buf,count);
        Paging::EnableKernel();

        file->pipe.is_received = 0;

        ctx->rdi = count;

        return 0;

    }

    char* dest_buf = (char*)PMM::VirtualBigAlloc(SIZE_TO_PAGES(count));

    __prepare_file_content_syscall(dest_buf,count,ctx->cr3);

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(dest_buf,buf,count);
    Paging::EnableKernel();

    VFS::Write(dest_buf,file->path_point,count,0);

    file->seek_offset += count;

    PMM::VirtualFree(dest_buf);

    ctx->rdx = count;

    return 0;


}

int syscall_close(int_frame_t* ctx) {
    int fd = ctx->rdi;

    process_t* proc = CpuData::Access()->current;
    fd_t* file = FD::Search(proc,fd);

    if(!file)
        return -2;

    if(file->index < 3)
        return -1;
        
    file->type = FD_NONE;

    return 0;

}

int syscall_dump_tmpfs(int_frame_t* ctx) {
    tmpfs_dump();
    return 0;
}

int syscall_mmap(int_frame_t* ctx) {
    uint64_t hint = ctx->rdi;
    uint64_t size = ctx->rsi;

    process_t* proc = CpuData::Access()->current;

    if(!size) return -1;

    uint64_t size_in_pages = ALIGNPAGEUP(size) / 4096; 

    uint64_t allocated = PMM::BigAlloc(size_in_pages);
    if(!allocated) return -2;

    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(ctx->cr3);
    
    if(!hint) 
        hint = (uint64_t)VMM::Alloc(proc,size,PTE_RW | PTE_PRESENT | PTE_USER);    
    else
        VMM::CustomAlloc(proc,hint,size,PTE_RW | PTE_PRESENT | PTE_USER);

    ctx->rdx = hint;
    return 0;

}

int syscall_free(int_frame_t* ctx) {
    
    uint64_t ptr = ctx->rdi;
    uint64_t size = ctx->rsi;
    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(ctx->cr3);

    if(!ptr) return -1;
    if(!size) return -2;
    uint64_t size_in_pages = ALIGNPAGEUP(size) / 4096;     
    uint64_t phys = Paging::PhysFromVirt(cr3,ptr);

    Paging::Unmap(cr3,ptr,size_in_pages);
    PMM::Free(phys);
    return 0;

}

int syscall_isatty(int_frame_t* ctx) {

    int fd = ctx->rdi;

    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);

    if(file->path_point[0] != '\0') {
        if(!String::strcmp("/dev/tty",file->path_point))
            return 0;
        else 
            return 25;
    }
    
    return 25;

}

int syscall_fork(int_frame_t* ctx) {

    //Log("fork()\n");
    process_t* parent = CpuData::Access()->current;
    uint64_t id = Process::createThread(ctx->rip,parent->id);

    ctx->rdx = id;
    ctx->rax = 0;

    process_t* new_proc = Process::ByID(id);

    String::memcpy(&new_proc->ctx,ctx,sizeof(int_frame_t));
    new_proc->ctx.rsp = CpuData::Access()->user_stack;
    new_proc->ctx.ss = 0x18 | 3;
    new_proc->ctx.cs = 0x20 | 3;

    uint64_t* virt = (uint64_t*)HHDM::toVirt(new_proc->ctx.cr3);
    Paging::alwaysMappedMap(virt);
    Paging::Kernel(virt);

    VMM::Init(new_proc);
    new_proc->user_stack_start = parent->user_stack_start;
    VMM::Clone(new_proc,parent);
    new_proc->fs_base = parent->fs_base;

    uint64_t new_stack = PMM::BigAlloc(PROCESS_STACK_SIZE + 1);
    uint64_t parent_stack = HHDM::toVirt(VMM::Get(parent,parent->user_stack_start)->phys);

    String::memcpy((void*)HHDM::toVirt(new_stack),(void*)parent_stack,(PROCESS_STACK_SIZE + 1) * PAGE_SIZE);

    VMM::Reload(new_proc);

    new_proc->ctx.rdx = 0;

    new_proc->cwd = cwd_get((const char*)parent->name);
    
    char* name = (char*)PMM::VirtualAlloc();
    String::memcpy(name,parent->name,String::strlen(parent->name));
    new_proc->name = name;

    Process::WakeUp(id);

    return 0;
}

int syscall_getpid(int_frame_t* ctx) {
    return CpuData::Access()->current->id;
}

int syscall_dump_memory(int_frame_t* ctx) {
    VMM::Dump(CpuData::Access()->current);
    return 0;
}

//[[gnu::weak]] int sys_execve(const char *path, char *const argv[], char *const envp[]);

uint64_t __elf_get_length2(char** arr) {
    uint64_t counter = 0;

    while(arr[counter])
        counter++;

    return counter;
}

int syscall_exec(int_frame_t* ctx) {
    
    char* path = (char*)ctx->rdi;
    char** argv = (char**)ctx->rsi;
    char** envp = (char**)ctx->rdx;

    process_t* proc = CpuData::Access()->current;

    uint64_t argv_length = 0;
    uint64_t envp_length = 0;

    //Paging::MemoryEntry((uint64_t*)HHDM::toVirt(ctx->cr3),LIMINE_MEMMAP_FRAMEBUFFER,PTE_RW | PTE_PRESENT);
    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));

    argv_length = __elf_get_length2((char**)argv);
    envp_length = __elf_get_length2((char**)envp);

    Paging::EnableKernel();

    char** stack_argv = (char**)KHeap::Malloc(8 * (argv_length + 1));
    char** stack_envp = (char**)KHeap::Malloc(8 * (envp_length + 1));

    char stack_path[1024];

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));

    String::memset(stack_argv,0,8 * (argv_length + 1));
    String::memset(stack_envp,0,8 * (envp_length + 1));

    for(int i = 0;i < argv_length; i++) {

        char* str = argv[i];

        char* new_str = (char*)KHeap::Malloc(String::strlen(str) + 1);

        String::memcpy(new_str,str,String::strlen(str));

        stack_argv[i] = new_str;

    }

    for(int i = 0;i < envp_length; i++) {

        char* str = envp[i];

        char* new_str = (char*)KHeap::Malloc(String::strlen(str) + 1);

        String::memcpy(new_str,str,String::strlen(str));

        stack_envp[i] = new_str;

    }

    String::memset(stack_path,0,1024);
    String::memcpy(stack_path,path,String::strlen(path));

    Paging::EnableKernel();

    char* first = proc->cwd;
    if(!first)
        first = "/";

    char buf[1024];
    char* path1 = (char*)buf;
    String::memset(buf,0,1024);

    int ptr = String::strlen(first);
    String::memcpy(buf,first,ptr);

    buf[ptr] = '/';
    buf[ptr + 1] = 'd';

    resolve_path(stack_path,buf,path1);

    VMM::Free(proc);

    VMM::Init(proc);

    filestat_t stat;

    int status = VFS::Stat(path1,(char*)&stat);

    if(!status) {
        //Log("alloc\n");
        char* elf = (char*)PMM::VirtualBigAlloc(CALIGNPAGEUP(stat.size,4096) / 4096);
        //Log("reading %s 0x%p\n",path1,elf);
        int status1 = VFS::Read(elf,path1,0);

        if(!elf && status1) {
            if(elf)
                PMM::VirtualFree(elf);

            return -1;

        } else {
            String::memset(&proc->ctx,0,sizeof(int_frame_t));
            
            proc->ctx.cs = 0x20 | 3;
            proc->ctx.ss = 0x18 | 3;
            proc->ctx.rflags = (1 << 9); // setup IF

            fd_t* current_fd = (fd_t*)proc->start_fd;
            while(current_fd) {

                current_fd->seek_offset = 0;

                current_fd = current_fd->next;
            }

            VMM::Reload(proc);

            Process::loadELFProcess(proc->id,path1,(uint8_t*)elf,stack_argv,stack_envp);
            for(int i = 0;i < argv_length; i++) {

                KHeap::Free(stack_argv[i]);
        
            }
        
            for(int i = 0;i < envp_length; i++) {
        
                KHeap::Free(stack_envp[i]);
        
            }
        
            KHeap::Free(stack_argv);
            KHeap::Free(stack_envp);
            //PMM::VirtualFree(path1);

            PMM::VirtualFree(elf);

            schedulingSchedule(0);
        }

        
    } 

    for(int i = 0;i < argv_length; i++) {

        KHeap::Free(stack_argv[i]);

    }

    for(int i = 0;i < envp_length; i++) {

        KHeap::Free(stack_envp[i]);

    }

    Log(LOG_LEVEL_DEBUG,"exec() error in process %d, can't find/load a need file\n",proc->id);

    Process::Kill(proc,-1);

    KHeap::Free(stack_argv);
    KHeap::Free(stack_envp);
    PMM::VirtualFree(path1);

    schedulingSchedule(0);
    

}

int syscall_getcwd(int_frame_t* ctx) {
    char* buf = (char*)ctx->rdi;
    uint64_t size = ctx->rsi;

    char cwd[2048];

    String::memset(cwd,0,2048);

    String::memcpy(cwd,CpuData::Access()->current->cwd ? CpuData::Access()->current->cwd : 0,String::strlen(CpuData::Access()->current->cwd ? CpuData::Access()->current->cwd : 0));

    uint64_t actual_size = (size > String::strlen(CpuData::Access()->current->cwd ? CpuData::Access()->current->cwd : 0)) ? String::strlen(CpuData::Access()->current->cwd ? CpuData::Access()->current->cwd : 0) : size;

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(buf,cwd,actual_size);

    Paging::EnableKernel();

    return 0;

}

int syscall_getppid(int_frame_t* ctx) {
    return CpuData::Access()->current->parent_process;
}

int syscall_gethostname(int_frame_t* ctx) {
    uint64_t buf = ctx->rdi;
    uint64_t size = ctx->rsi;

    const char* host_name = "orange-pc";

    uint64_t actual_size = size > String::strlen((char*)host_name) ? size : String::strlen((char*)host_name);

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memset((void*)buf,0,actual_size);
    String::memcpy((void*)buf,host_name,actual_size == size ? String::strlen((char*)host_name) : actual_size);
    Paging::EnableKernel();

    return 0;

}

int syscall_stat(int_frame_t* ctx) {

    uint64_t fd = ctx->rdi;
    stat_t* out = (stat_t*)ctx->rsi;

    process_t* proc = CpuData::Access()->current;

    fd_t* fd_s = FD::Search(proc,fd);

    if(!fd_s)
        return -100;

    filestat_t stat;
    int st = VFS::Stat(fd_s->path_point,(char*)&stat);

    if(st && st != -15 && st != 5)
        return st;

    if(st == 5) {
        Log(LOG_LEVEL_DEBUG,"File %s is not found\n",fd_s->path_point);
        return 0;
    }
        
    

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));

    String::memset(out,0,sizeof(stat_t));
    if(st != -15) {
        out->st_size = stat.size;
        out->st_mode = stat.type == VFS_TYPE_FILE ? S_IFREG : S_IFDIR;
    } else {
        out->st_size = 0;
        out->st_mode = S_IFREG;
    }
    
    Paging::EnableKernel();

    //Log(LOG_LEVEL_DEBUG,"Stating %s (size: %d)\n",fd_s->path_point,stat.size);

    return 0;

}

int syscall_dup(int_frame_t* ctx) {
    int fd = ctx->rdi;
    process_t* proc = CpuData::Access()->current;

    fd_t* fd_s = FD::Search(proc,fd);
    if(!fd_s)
        return -200;

    int new_fd = FD::Create(proc,fd_s->type == FD_PIPE ? 1 : 0);
    fd_t* fd_new = FD::Search(proc,new_fd);

    String::memcpy(fd_new,fd_s,sizeof(fd_t));
    fd_new->index = new_fd;
    fd_new->next = 0;

    ctx->rdx = new_fd;
    return 0;
}

int syscall_kill(int_frame_t* ctx) {
    int pid = ctx->rdi;
    Process::Kill(Process::ByID(pid),ctx->rsi);
    return 0;
}

syscall_t syscall_table[] = {
    {1,syscall_exit},
    {2,syscall_debug_print},
    {3,syscall_futex_wait},
    {4,syscall_futex_wake},
    {5,syscall_tcb_set},
    {6,syscall_dump_tmpfs},
    {7,syscall_open},
    {8,syscall_seek},
    {9,syscall_read},
    {10,syscall_write},
    {11,syscall_close},
    {12,syscall_mmap},
    {13,syscall_free},
    {14,syscall_isatty},
    {15,syscall_fork},
    {16,syscall_exec},
    {17,syscall_getpid},
    {18,syscall_dump_memory},
    {19,syscall_exec},
    {20,syscall_getcwd},
    {21,syscall_getppid},
    {22,syscall_gethostname},
    {23,syscall_stat},
    {24,syscall_dup},
    {25,syscall_kill}
};

syscall_t* syscall_find_table(int num) {
    for(int i = 0;i < sizeof(syscall_table) / sizeof(syscall_t);i++) {
        if(syscall_table[i].num == num)
            return &syscall_table[i];
    }
    return 0; // :(
}

extern "C" void c_syscall_handler(int_frame_t* ctx) {
    Paging::EnableKernel();
    syscall_t* sys = syscall_find_table(ctx->rax);

    if(sys == 0) {
        ctx->rax = -1;
        return;
    }

    ctx->rax = sys->func(ctx);

    //Log(LOG_LEVEL_DEBUG,"Syscall %d with ret %d\n",sys->num,ctx->rax);

    return;
}

// end of syscalls

void Syscall::Init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,(1 << 9)); // disable only IF
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}
