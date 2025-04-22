
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
#include <other/other.hpp>

extern "C" void syscall_handler();

// simple fd implementation

fd_t head_fd;
fd_t stdout_fd;
fd_t stderr_fd;

int fd_ptr = 3; 

fd_t* last_fd;
char is_fd_initializied = 0;

int create_fd(char* path) {

    if(!is_fd_initializied) {

        const char* stdout = "/dev/tty";
        const char* stdin = "/dev/kbd";
        const char* stderr = "/dev/serial";

        head_fd.index = 0;
        head_fd.is_in_use = 1;
        head_fd.next = &stdout_fd;
        
        String::memcpy(head_fd.path,stdin,String::strlen((char*)stdin));

        stdout_fd.index = 1;
        stdout_fd.is_in_use = 1;
        stdout_fd.next = &stderr_fd;

        String::memcpy(stdout_fd.path,stdout,String::strlen((char*)stdout));

        stderr_fd.index = 2;
        stderr_fd.is_in_use = 1;
        stderr_fd.next = 0;

        String::memcpy(stderr_fd.path,stderr,String::strlen((char*)stderr));

        last_fd = &stderr_fd;

        is_fd_initializied = 1;
    }

    fd_t* fd = &head_fd;

    char success = 0;

    while (fd)
    {

        if(!fd->is_in_use) {
            success = 1;
            fd->is_in_use = 1;
            break;
        }

        fd = fd->next;
    }
    
    if(!success) {
        fd = (fd_t*)PMM::VirtualAlloc();
        fd->index = fd_ptr++;
    } 
    
    fd->is_in_use = 1;
    fd->seek_offset = 0;
    last_fd->next = fd;
    last_fd = fd;

    String::memcpy(fd->path,path,String::strlen((char*)path));

    return fd->index;

}

fd_t* find_fd(int index) {

    fd_t* fd = &head_fd;

    while (fd)
    {
        
        if(fd->index == index)
            return fd;

        fd = fd->next;
    }

    return 0;
    
}

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

    char* first = proc->cwd;
    if(!first)
        first = "/";

    char* path = join_paths(first,buffer);

    int fd = create_fd(path);
    fd_t* fd_s = find_fd(fd);

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

    fd_t* file = find_fd(fd);
    filestat_t stat;

    if(VFS::Stat(file->path,(char*)&stat)) {
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

    ctx->rdi = file->seek_offset;

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

int syscall_read(int_frame_t* ctx) {

    int fd = ctx->rdi;
    void* buf = (void*)ctx->rsi;
    uint64_t count = ctx->rdx;
    long* bytes_read = (long*)ctx->rcx;

    fd_t* file = find_fd(fd);
    filestat_t stat;

    if(VFS::Stat(file->path,(char*)&stat)) {
        return -4;
    }

    if(!file)
        return -1;

    if(fd < 3)
        return -3;

    long seek_off = file->seek_offset;

    uint64_t actual_size = 0;
    char is_file_bigger = 0;

    if(count < stat.size) {
        actual_size = count;
        is_file_bigger = 1;
    } else if(count > stat.size) {
        actual_size = stat.size;
        is_file_bigger = 0;
    } else {
        actual_size = count;
        is_file_bigger = 1;
    }

    char* actual_src = stat.content;

    if(seek_off > 0)
        actual_src = (char*)((uint64_t)actual_src + seek_off);
    else if(seek_off < 0)
        actual_size += seek_off;

    if(stat.content) {
        __prepare_file_content_syscall(stat.content,stat.size,ctx->cr3);
    } else {
        actual_src = "";
        actual_size = 0;
    }

    Paging::EnablePaging((uint64_t*)HHDM::toVirt(ctx->cr3));
    String::memset(buf,0,count);
    String::memcpy(buf,actual_src,actual_size);
    Paging::EnableKernel();

    ctx->rdi = actual_size;

    return 0;

}

int syscall_dump_tmpfs(int_frame_t* ctx) {

    tmpfs_dump();
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
    {9,syscall_read}
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
    
    return;
}

// end of syscalls

void Syscall::Init() {
    __wrmsr(STAR_MSR,(0x08ull << 32) | (0x10ull << 48));
    __wrmsr(LSTAR,(uint64_t)syscall_handler);
    __wrmsr(STAR_MASK,(1 << 9)); // disable only IF
    __wrmsr(EFER,__rdmsr(EFER) | 1);
}