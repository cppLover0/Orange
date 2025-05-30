
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
#include <generic/limineA/limineinfo.hpp>
#include <other/debug.hpp>

extern "C" void syscall_handler();

// syscalls

uint64_t address_max = 0;

char __syscall_is_safe_pointer(void* p) {
    uint64_t pp = (uint64_t)p;
    if(!address_max) {
        LimineInfo info;
        address_max = info.hhdm_offset;
    }

    if(pp >= address_max)
        return 0;

    return 1;

}

#define __syscall_is_safe(x) \
    if(!__syscall_is_safe_pointer(x)) \
        return EFAULT;

int syscall_exit(int_frame_t* ctx) {

    process_t* proc = CpuData::Access()->current;

    fd_t* fd = (fd_t*)proc->start_fd;
    while(fd) {
        fd = fd->next;
        if(fd) {
            if(fd->is_pipe_pointer && fd->type == FD_PIPE && fd->pipe_side == PIPE_SIDE_WRITE) {
                fd->p_pipe->connected_pipes--;
                if(fd->p_pipe->connected_pipes == 0) {
                    fd->p_pipe->is_received = 0;
                }
            }
                
            String::memset(fd,0,4096);
            PMM::VirtualFree(fd);
        }
    }

    Process::Kill(proc,ctx->rdi);

    //Log(LOG_LEVEL_DEBUG,"Process %d is died with code %d !\n",proc->id,proc->return_status);

    schedulingSchedule(ctx);
    
}

int syscall_debug_print(int_frame_t* ctx) {

    uint64_t size = ctx->rsi;
    char buffer[1024];
    
    String::memset(buffer,0,1024);

    char* ptr = &buffer[0];

    String::memset(ptr,0,size);

    char* src = (char*)ctx->rdi;

    __syscall_is_safe(src);

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

    __syscall_is_safe(name);
    __syscall_is_safe(fdout);

    int flags = ctx->rdx;

    process_t* proc = CpuData::Access()->current;

    if(!name && !fdout)
        return -2;

    char buffer[4096];

    String::memset(buffer,0,4096);

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(buffer,name,String::strlen(name));
    Paging::EnableKernel();    

    if(buffer[String::strlen(buffer) - 1] == '/' && String::strcmp(buffer,"/"))
        buffer[String::strlen(buffer) - 1] = '\0';

    char* first = proc->cwd;
    if(!first)
        first = "/";

    char buf[1024];
    char* path = (char*)buf;
    String::memset(buf,0,1024);

    char buf1[1024];
    char* path1 = (char*)buf1;
    String::memset(buf1,0,1024);

    int ptr = String::strlen(first);
    String::memcpy(path1,first,ptr);

    resolve_path(buffer,path1,path,1);    

    if(flags & O_CREAT)
        VFS::Touch(path);
    
    filestat_t zx;
    int stt = VFS::Stat(path,(char*)&zx); 

    if(stt && stt != -15)
        return ENOENT;

    if((flags & O_TRUNC) && ((flags & O_WRONLY) || (flags & O_RDWR)))
        VFS::Write({0},path,1,0,0);

    int fd = FD::Create(proc,0);
    fd_t* fd_s = FD::Search(proc,fd);

    if(!fd_s)
        return -3;

    String::memset(fd_s->path_point,0,2048);
    String::memcpy(fd_s->path_point,path,String::strlen(path));

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
        return EBADF;

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

void __prepare_file_content_syscall(char* content,uint64_t size,uint64_t phys_cr3) {

    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(phys_cr3);
    uint64_t phys_file = HHDM::toPhys((uint64_t)content);
    uint64_t aligned_size_in_pages = ALIGNPAGEUP(size) / PAGE_SIZE;
    for(uint64_t i = 0;i < aligned_size_in_pages;i++) {
        Paging::HHDMMap(cr3,phys_file + (i * PAGE_SIZE),PTE_PRESENT | PTE_RW);
    }


}

extern "C" void syscall_end(int_frame_t* ctx);

// sorry for this shitcode
extern "C" int syscall_read_stage_2(int_frame_t* ctx,fd_t* file) {

    void* buf = (void*)ctx->rsi;
    uint64_t count = ctx->rdx;
    uint64_t real_count = ctx->rdx;

    //Log(LOG_LEVEL_DEBUG,"Reading from /dev/tty\n");

    if(file->path_point[0])
        VFS::AskForPipe(file->path_point,&file->pipe);

    process_t* proc = CpuData::Access()->current;

    if(!file->is_pipe_pointer) {
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
                ctx->rdx = file->pipe.buffer_size;
                file->pipe.buffer_size = 0;
                ctx->rax = 0;

                proc->is_eoi = 1;

                Lapic::EOI();
                syscall_end(ctx);

            } else {
                asm volatile("int $32");
            }
        }
    } else {
        while(1) {

            if(file->p_pipe->is_eof) {
                ctx->rdx = 0;
                ctx->rax = 0;
                Lapic::EOI();
                syscall_end(ctx);
            }

            if(!file->p_pipe->is_received) {
                if(file->p_pipe->buffer_size - file->p_pipe->buffer_read_ptr < count)
                    count = file->p_pipe->buffer_size - file->p_pipe->buffer_read_ptr;

                char* pipe_buffer = file->p_pipe->buffer;

                pipe_buffer = (char*)((uint64_t)pipe_buffer + file->p_pipe->buffer_read_ptr);
                file->p_pipe->buffer_read_ptr += count;

                __prepare_file_content_syscall(file->p_pipe->buffer,count,ctx->cr3);
                Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
                String::memset(buf,0,real_count);
                String::memcpy(buf,pipe_buffer,count);
                Paging::EnableKernel();
                
                ctx->rdx = count;
                ctx->rax = 0;
                if(file->p_pipe->buffer_size <= file->p_pipe->buffer_read_ptr) {
                    file->p_pipe->is_eof = 1;
                    file->p_pipe->is_received = 1;
                } else {
                    file->p_pipe->is_received = 0;
                }

                proc->is_eoi = 1;

                Lapic::EOI();
                syscall_end(ctx);

            } else {
                asm volatile("int $32");
            }
        }
    }

}

extern "C" void syscall_read_stage_2_asm(uint64_t new_stack,int_frame_t* ctx,fd_t* file);

int syscall_read(int_frame_t* ctx) {

    int fd = ctx->rdi;
    void* buf = (void*)ctx->rsi;

    __syscall_is_safe(buf);

    uint64_t count = ctx->rdx;
    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);
    filestat_t stat;

    extern termios_t tty_termios;

    if(!file)
        return -1;

    if(file->type == FD_NONE)
        return EBADF;

    if(file->type == FD_PIPE) {
        file->pipe.buffer_size = 0;
        
        //proc->is_cli = 0;
        if(!file->is_pipe_pointer) {

            if(file->pipe.type == PIPE_WAIT || (!String::strcmp("/dev/tty",file->path_point)&& (tty_termios.c_lflag & ICANON))) {
                proc->is_eoi = 0;
                String::memcpy(proc->syscall_wait_ctx,ctx,sizeof(int_frame_t));
                syscall_read_stage_2_asm((uint64_t)proc->wait_stack + 4096,proc->syscall_wait_ctx,file);
                return -20;
            }

            if(file->pipe.type == PIPE_INSTANT) {
                
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
                    ctx->rdx = file->pipe.buffer_size;

                    file->pipe.buffer_size = 0;
            
                    proc->is_eoi = 1;

                    return 0;

            
                } else {
                    ctx->rdx = 0;
                    return 0;
                }

            }

        } else {

            if(file->pipe_side != PIPE_SIDE_READ)
                return EFAULT;

            if(file->p_pipe->type == PIPE_WAIT || (!String::strcmp("/dev/tty",file->path_point)&& (tty_termios.c_lflag & ICANON))) {
                proc->is_eoi = 0;
                String::memcpy(proc->syscall_wait_ctx,ctx,sizeof(int_frame_t));
                syscall_read_stage_2_asm((uint64_t)proc->wait_stack + 4096,proc->syscall_wait_ctx,file);
                return -20;
            }

            if(file->p_pipe->type == PIPE_INSTANT) {
                
                if(!file->p_pipe->is_used) 
                    VFS::AskForPipe(file->path_point,file->p_pipe);

                if(!file->p_pipe->is_received) {

                    if(file->p_pipe->buffer_size < count)
                        count = file->p_pipe->buffer_size;
            
                    char* pipe_buffer = file->p_pipe->buffer;
            
                    __prepare_file_content_syscall(file->p_pipe->buffer,count,ctx->cr3);

                    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
                    String::memcpy(buf,pipe_buffer,count);
                    Paging::EnableKernel();

                    file->p_pipe->is_received = 1;
                    ctx->rdx = file->p_pipe->buffer_size;

                    file->p_pipe->buffer_size = 0;
            
                    proc->is_eoi = 1;

                    return 0;

            
                } else {
                    ctx->rdx = 0;
                    return 0;
                }

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

    if(!is_file_bigger)
        actual_size = stat.size - seek_off;

    if(!is_file_bigger)
        file->seek_offset += actual_size;
    else
        file->seek_offset += count;

    if(file->seek_offset > stat.size) {
        ctx->rdx = 0;
        return 0;
    }

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memset(buf,0,count);
    if(!is_file_bigger) {
        String::memcpy(buf,actual_src,actual_size);
    } else {
        String::memcpy(buf,actual_src,count);
    }
    Paging::EnableKernel();

    if(!is_file_bigger)
        ctx->rdx = actual_size;
    else
        ctx->rdx = count;

    return 0;

}

int syscall_write(int_frame_t* ctx) {

    int fd = ctx->rdi;
    void* buf = (void*)ctx->rsi;

    __syscall_is_safe(buf);

    uint64_t count = ctx->rdx;

    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);
    filestat_t stat;

    if(!file)
        return EBADF;

    if(file->type == FD_PIPE) {
        
        if(count > (1024 * 64))
            count = (1024 * 64) - 1;

        if(!file->is_pipe_pointer) {
            char* pipe_buffer = file->pipe.buffer;
            file->pipe.buffer_size = count;

            __prepare_file_content_syscall(pipe_buffer,count,ctx->cr3);

            Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
            String::memcpy(pipe_buffer,buf,count);
            Paging::EnableKernel();

            file->pipe.is_received = 0;
        } else {
            char* pipe_buffer = file->p_pipe->buffer;

            pipe_buffer = (char*)((uint64_t)pipe_buffer + file->p_pipe->buffer_size);
            file->p_pipe->buffer_size += count;

            
            __prepare_file_content_syscall(pipe_buffer,count,ctx->cr3);

            Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
            String::memcpy(pipe_buffer,buf,count);
            Paging::EnableKernel();
            
        }

        ctx->rdi = count;

        return 0;

    }

    char* dest_buf = (char*)PMM::VirtualBigAlloc(SIZE_TO_PAGES(count));

    __prepare_file_content_syscall(dest_buf,count,ctx->cr3);

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(dest_buf,buf,count);
    Paging::EnableKernel();

    //Log(LOG_LEVEL_DEBUG,"writ %d %s\n",fd,file->path_point);

    VFS::Write(dest_buf,file->path_point,count,0,file->seek_offset);

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



    if(fd < 3) {
        // restore state
        String::memset(file->path_point,0,2048);
        String::memcpy(file->path_point,"/dev/tty",String::strlen("/dev/tty"));
        file->type = file->old_type;
        file->pipe.buffer = file->pipe.old_buffer;
    } else if(file->type == FD_PIPE) {
        if(file->is_pipe_pointer) {
            if(file->pipe_side == PIPE_SIDE_WRITE && !file->is_pipe_dup2) {
                if(file->p_pipe->connected_pipes == 1) {
                    file->p_pipe->is_received = 0;
                }
                file->p_pipe->connected_pipes--;
            }
            file->p_pipe = file->old_p_pipe;
            file->is_pipe_pointer = file->old_is_pipe_pointer;
        
        } else {
            PMM::VirtualFree(file->pipe.buffer);
        }
        file->type = FD_NONE;
    } else {
        file->type = FD_NONE;
    }
    

    return 0;

}

int syscall_dump_tmpfs(int_frame_t* ctx) {
    tmpfs_dump();
    return 0;
}

int syscall_mmap(int_frame_t* ctx) {
    uint64_t hint = ctx->rdi;

    __syscall_is_safe((void*)hint);

    uint64_t size = ctx->rsi;
    int fd = ctx->rdx;

    process_t* proc = CpuData::Access()->current;

    if(!size) return -1;

    if(fd && fd > 0) {

        fd_t* file = FD::Search(proc,fd);

        if(file) {
            filestat_t stat;
            int stat_st = VFS::Stat(file->path_point,(char*)&stat);

            if(stat_st)
                return ENOSYS;

            uint64_t virt = (uint64_t)VMM::Map(proc,(uint64_t)stat.content,stat.size,PTE_RW | PTE_PRESENT | PTE_USER);
            ctx->rdx = virt;
            return 0;

        } else
            return ENOENT;
    }

    uint64_t size_in_pages = ALIGNPAGEUP(size) / 4096; 

    uint64_t allocated = PMM::BigAlloc(size_in_pages);
    if(!allocated) return -2;

    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(ctx->cr3);
    
    if(!hint) 
        hint = (uint64_t)VMM::Alloc(proc,size,PTE_RW | PTE_PRESENT | PTE_USER);    
    else
        VMM::CustomAlloc(proc,hint,size,PTE_RW | PTE_PRESENT | PTE_USER);

    //Serial::printf(" %p ",hint);

    ctx->rdx = hint;
    return 0;

}

int syscall_free(int_frame_t* ctx) {
    
    uint64_t ptr = ctx->rdi;
    uint64_t size = ctx->rsi;
    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(ctx->cr3);

    __syscall_is_safe((void*)ptr);

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
            return ENOTTY;
    }
    
    return ENOTTY;

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

    new_proc->cwd = (char*)PMM::VirtualAlloc();
    String::memset(new_proc->cwd,0,4096);
    String::memcpy(new_proc->cwd,parent->cwd,String::strlen(parent->cwd));

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

    resolve_path(stack_path,buf,path1,1);

    VMM::Free(proc);

    VMM::Init(proc);

    filestat_t stat;

    int status = VFS::Stat(path1,(char*)&stat);

    if(!status && stat.type == VFS_TYPE_FILE) {
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

            int status1 = Process::loadELFProcess(proc->id,path1,(uint8_t*)elf,stack_argv,stack_envp);

            if(!status1) {
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

            PMM::VirtualFree(elf);

            
        }

        
    } 

    for(int i = 0;i < argv_length; i++) {

        KHeap::Free(stack_argv[i]);

    }

    for(int i = 0;i < envp_length; i++) {

        KHeap::Free(stack_envp[i]);

    }

    Process::Kill(proc,ENOENT);

    KHeap::Free(stack_argv);
    KHeap::Free(stack_envp);
    PMM::VirtualFree(path1);

    schedulingSchedule(0);
    

}

int syscall_getcwd(int_frame_t* ctx) {
    char* buf = (char*)ctx->rdi;
    uint64_t size = ctx->rsi;

    __syscall_is_safe(buf);

    char cwd[2048];

    String::memset(cwd,0,2048);
    String::memcpy(cwd,CpuData::Access()->current->cwd,String::strlen(CpuData::Access()->current->cwd));

    if(size < String::strlen(cwd))
        return ERANGE;

   

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(buf,cwd,String::strlen(cwd));
    Paging::EnableKernel();

    return 0;

}

int syscall_getppid(int_frame_t* ctx) {
    return CpuData::Access()->current->parent_process;
}

int syscall_gethostname(int_frame_t* ctx) {
    uint64_t buf = ctx->rdi;
    uint64_t size = ctx->rsi;

    __syscall_is_safe((void*)buf);

    const char* host_name = "orange-pc";

    if(String::strlen((char*)host_name) < size)
        return ERANGE;

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memset((void*)buf,0,size);
    String::memcpy((void*)buf,host_name,String::strlen((char*)host_name));
    Paging::EnableKernel();

    return 0;

}

int syscall_stat(int_frame_t* ctx) {

    uint64_t fd = ctx->rdi;
    stat_t* out = (stat_t*)ctx->rsi;

    __syscall_is_safe(out);

    process_t* proc = CpuData::Access()->current;

    fd_t* fd_s = FD::Search(proc,fd);

    if(!fd_s)
        return EBADF;

    filestat_t stat;
    int st = VFS::Stat(fd_s->path_point,(char*)&stat);


    if(fd_s->type == FD_PIPE)
        st = -15;

    if(st && st != -15)
        return ENOENT;

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

    return 0;

}

int syscall_dup(int_frame_t* ctx) {
    int fd = ctx->rdi;
    process_t* proc = CpuData::Access()->current;

    fd_t* fd_s = FD::Search(proc,fd);
    if(!fd_s)
        return EBADF;

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

// int fd, unsigned long request, void *arg, int *result

int syscall_ioctl(int_frame_t* ctx) {
    int fd = ctx->rdi;
    uint64_t request = ctx->rsi;
    void* arg = (void*)ctx->rdx;

    __syscall_is_safe(arg);

    process_t* proc = CpuData::Access()->current;

    int size = 0;
    switch(request) {
        case TCGETS:
            size = sizeof(termios_t);
            break;
        case TCSETS:
            size = sizeof(termios_t);
            break;
        case TIOCGWINSZ:
            size = sizeof(winsize_t);
            break;
        default:
            return 38;
    }

    char temp_buffer[size + 1];
    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(temp_buffer,arg,size);
    Paging::EnableKernel();

    fd_t* file = FD::Search(proc,fd);
    if(!file)
        return 1;

    int status = VFS::Ioctl(file->path_point,request,temp_buffer,0);

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memcpy(arg,temp_buffer,size);
    Paging::EnableKernel();

    return status;

}

extern "C" void syscall_waitpid_stage2_asm(uint64_t new_stack,int_frame_t* ctx);

extern "C" void syscall_waitpid_stage2(int_frame_t* ctx) {
    
    process_t* hproc = CpuData::Access()->current;
    int pid = ctx->rdi;
    int* ret_pid = (int*)ctx->rsi;

    if(pid > 0) {
        process_t* target_proc = Process::ByID(pid);
        while(target_proc->status != PROCESS_STATUS_KILLED) {asm volatile("int $32");}
        target_proc->is_waitpid_used = 1;
        int ned_pid = target_proc->id;
        ctx->rax = 0;
        ctx->rdx = target_proc->return_status;

        Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
        *ret_pid = ned_pid;
        Paging::EnableKernel();

        hproc->is_eoi = 1;
        Lapic::EOI();
        syscall_end(ctx);
    } else {
        while(1) {
            extern process_t* head_proc;

            process_t* proc = head_proc;
            while(proc) {
                if(proc->parent_process == hproc->id && proc->status == PROCESS_STATUS_KILLED && !proc->is_waitpid_used) {
                    proc->is_waitpid_used = 1;
                    
                    ctx->rax = 0;
                    ctx->rdx = proc->return_status;

                    int ned_pii = proc->id;

                    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
                    *ret_pid = ned_pii;
                    Paging::EnableKernel();

                    hproc->is_eoi = 1;
                    Lapic::EOI();
                    syscall_end(ctx);

                }
                proc = proc->next;
            }
            asm volatile("int $32");
        }
    }

}

int syscall_waitpid(int_frame_t* ctx) {
    int pid = ctx->rdi;
    process_t* hproc = CpuData::Access()->current;

    int success = 0;

    if(pid < -1 || pid == 0)
        return ENOSYS;

    if(pid > 0) {
        process_t* nedporc = Process::ByID(pid);
        if(!nedporc)
            return ESRCH;

        success = 1;

    }

    // search for available processes
    extern process_t* head_proc;

    process_t* proc = head_proc;
    while(proc) {
        if(proc->parent_process == hproc->id && !proc->is_waitpid_used) {
            success = 1;
            break;
        }
        proc = proc->next;
    }

    if(success) {
        hproc->is_eoi = 0;
        String::memcpy(hproc->syscall_wait_ctx,ctx,sizeof(int_frame_t));
        syscall_waitpid_stage2_asm((uint64_t)hproc->wait_stack + 4096,hproc->syscall_wait_ctx);
    } else {
        ctx->rdx = 0;
        return 0;
    }

}

int syscall_dup2(int_frame_t* ctx) {
    int fd = ctx->rdi;
    int new_fd = ctx->rsi;

    process_t* proc = CpuData::Access()->current;

    fd_t* old_fd = FD::Search(proc,fd);
    fd_t* new_fd1 = FD::Search(proc,new_fd);

    if(!old_fd || !new_fd1)
        return ENOENT;

    String::memset(new_fd1->path_point,0,2048);
    String::memcpy(new_fd1->path_point,old_fd->path_point,2048);

    if(old_fd->type == FD_PIPE) {
        new_fd1->old_type = new_fd1->type;
        new_fd1->type = old_fd->type;
        if(old_fd->is_pipe_pointer) {
            old_fd->is_pipe_dup2 = 1;
            new_fd1->pipe.old_buffer = new_fd1->pipe.buffer;
            new_fd1->pipe.buffer = old_fd->pipe.buffer;
            new_fd1->old_is_pipe_pointer = new_fd1->is_pipe_pointer;
            new_fd1->is_pipe_pointer = 1;
            new_fd1->old_p_pipe = new_fd1->p_pipe;
            new_fd1->p_pipe = old_fd->p_pipe;
            new_fd1->pipe_side = old_fd->pipe_side;
        } else {
            new_fd1->pipe.old_buffer = new_fd1->pipe.buffer;
            new_fd1->pipe.buffer = old_fd->pipe.buffer;
        }
    } else {
        new_fd1->old_type = new_fd1->type;
        new_fd1->type = old_fd->type;
        new_fd1->pipe.old_buffer = new_fd1->pipe.buffer;
        new_fd1->pipe.buffer = old_fd->pipe.buffer;
    }

    if(FD::Search(proc,new_fd)->p_pipe != old_fd->p_pipe)
        Log(LOG_LEVEL_WARNING,"dup2 doesn't copied fds correctly\n");

    //Log(LOG_LEVEL_DEBUG,"new %s\n",new_fd1->path_point);

    return 0;

}

int syscall_fchdir(int_frame_t* ctx) {
    int fd = ctx->rdi;
    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);
    if(!file)
        return ENOENT;

    filestat_t stat;
    int status = VFS::Stat(file->path_point,(char*)&stat);

    if(status)
        return ENOENT;
    
    if(stat.type != VFS_TYPE_DIRECTORY)
        return ENOTDIR;

    String::memset(proc->cwd,0,4096);
    String::memcpy(proc->cwd,file->path_point,String::strlen(file->path_point));

    return 0;
}

int syscall_ttyname(int_frame_t* ctx) {

    int fd = ctx->rdi;
    void* buf = (void*)ctx->rsi;
    uint64_t size = ctx->rdx;  

    __syscall_is_safe(buf);

    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);
    if(!file)
        return EBADF;

    if(!buf)
        return EFAULT;

    if(!size)
        return EFAULT;

    if(!String::strcmp("/dev/tty",file->path_point)) {
        
        char sbuf[2048];
        String::memset(sbuf,0,2048);
        String::memcpy(sbuf,file->path_point,String::strlen(file->path_point));

        if(size <= String::strlen(sbuf))
            return ERANGE;

        Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
        String::memset(buf,0,size);
        String::memcpy(buf,sbuf,String::strlen(sbuf));
        Paging::EnableKernel();

        return 0;


    } else 
        return ENOTTY;

}

inline void __syscall_uname_helper_cpy(char* addr,const char* val) {
    String::memcpy(addr,val,String::strlen((char*)val));
}

int syscall_uname(int_frame_t* ctx) {

    utsname_t* uname = (utsname_t*)ctx->rdi;
    
    __syscall_is_safe(uname);
    if(!uname) 
        return EFAULT;

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memset(uname,0,sizeof(utsname_t));
    __syscall_uname_helper_cpy(uname->machine,"orange");
    Paging::EnableKernel();

    return 0;

}

int syscall_pipe(int_frame_t* ctx) {
    int* fds = (int*)ctx->rdi;
    int flags = ctx->rsi;

    __syscall_is_safe(fds);

    process_t* proc = CpuData::Access()->current;
    if(!fds)
        return EFAULT;

    int first = FD::Create(proc,0);
    int second = FD::Create(proc,0);

    fd_t* first_file = FD::Search(proc,first);
    fd_t* second_file = FD::Search(proc,second);

    pipe_t* shared_pipe = (pipe_t*)PMM::VirtualAlloc();

    shared_pipe->buffer = (char*)PMM::VirtualBigAlloc(16);
    shared_pipe->type = PIPE_WAIT;
    shared_pipe->connected_pipes = 1;
    shared_pipe->is_used = 1;
    shared_pipe->is_received = 1;
    shared_pipe->buffer_size = 0;
    shared_pipe->buffer_read_ptr = 0;

    first_file->is_pipe_pointer = 1;
    first_file->type = FD_PIPE;
    first_file->p_pipe = shared_pipe;
    first_file->pipe_side = PIPE_SIDE_READ;
    
    second_file->is_pipe_pointer = 1;
    second_file->type = FD_PIPE;
    second_file->p_pipe = shared_pipe;
    second_file->pipe_side = PIPE_SIDE_WRITE;

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    fds[0] = first;
    fds[1] = second;
    Paging::EnableKernel();

    String::memset(first_file->path_point,0,2048);
    String::memset(second_file->path_point,0,2048);

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
    {25,syscall_kill},
    {26,syscall_ioctl}, 
    {27,syscall_waitpid},
    {28,syscall_dup2},
    {29,syscall_fchdir},
    {30,syscall_ttyname},
    {31,syscall_uname},
    {32,syscall_pipe}
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

    CpuData::Access()->last_syscall = ctx->rax;

    //Serial::printf("sys %d\n",sys->num);

    if(sys == 0) {
        ctx->rax = -1;
        return;
    }

    ctx->rax = sys->func(ctx);

    return;
}

// end of syscalls

void Syscall::Init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,(1 << 9)); // disable only IF
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}
