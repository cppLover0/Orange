
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

extern "C" void syscall_handler();

// syscalls

int syscall_exit(int_frame_t* ctx) {

    process_t* proc = CpuData::Access()->current;
    proc->return_status = ctx->rdi;
    proc->status = PROCESS_STATUS_KILLED;
    
    if(proc->stack_start) {
        PMM::VirtualBigFree((void*)proc->stack_start,PROCESS_STACK_SIZE);
    }

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

        Log("%s\n",ptr);

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

    char* path = join_paths(first,buffer);
    
    int fd = FD::Create(proc,0);
    fd_t* fd_s = FD::Search(proc,fd);

    String::memset(fd_s->path_point,0,2048);
    String::memcpy(fd_s->path_point,path,String::strlen(path));

    if(!fd_s)
        return -3;

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    *fdout = fd;
    Paging::EnableKernel();

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
    long offset = ctx->rsi;
    int whence = ctx->rdx;
    long* new_offset = (long*)ctx->rcx;

    process_t* proc = CpuData::Access()->current;

    fd_t* file = FD::Search(proc,fd);
    filestat_t stat;

    if(VFS::Stat(file->path_point,(char*)&stat)) {
        return -4;
    }

    if(!file)
        return -1;

    if(!new_offset && !offset)
        return -2;

    if(fd < 3)
        return -3;


    switch (whence)
    {
        case SEEK_SET:
            file->seek_offset = offset;
            break;

        case SEEK_CUR:
            file->seek_offset += offset;
            break;

        case SEEK_END:
            file->seek_offset = offset;
            break;

        default:
            Log("Process %d, sys_seek, unhandled whence: %d.",CpuData::Access()->current->id,whence);
            return -5;

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

    if(file->type == FD_PIPE) {
        
        //proc->is_cli = 0;
        String::memcpy(proc->syscall_wait_ctx,ctx,sizeof(int_frame_t));
        syscall_read_stage_2_asm((uint64_t)proc->wait_stack + 4096,proc->syscall_wait_ctx,file);
        return -20;
        
    
    }

    if(VFS::Stat(file->path_point,(char*)&stat)) {
        
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

    if(stat.content) {
        __prepare_file_content_syscall(stat.content,stat.size,ctx->cr3);
    } else {
        actual_src = "";
        actual_size = 0;
    }

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

    PMM::VirtualBigFree(dest_buf,SIZE_TO_PAGES(count));

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


    

    if(!size) return -1;

    uint64_t size_in_pages = ALIGNPAGEUP(size) / 4096; 

    uint64_t allocated = PMM::BigAlloc(size_in_pages);
    if(!allocated) return -2;

    uint64_t* cr3 = (uint64_t*)HHDM::toVirt(ctx->cr3);
    if(!hint) hint = HHDM::toVirt(allocated);

    for(uint64_t i = 0;i < (size_in_pages * PAGE_SIZE); i += PAGE_SIZE) {
        Paging::Map(cr3,allocated + i,hint + i,PTE_PRESENT | PTE_RW | PTE_USER);
    }

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
    PMM::BigFree(phys,size_in_pages);
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
    {13,syscall_free}
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

    Log("Syscall %d\n",ctx->rax);
    
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