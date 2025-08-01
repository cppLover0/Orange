
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>

#include <generic/vfs/fd.hpp>

#include <etc/etc.hpp>
#include <etc/errno.hpp>

#include <etc/logging.hpp>

#include <cstdint>

syscall_ret_t sys_openat(int dirfd, const char* path, int flags) {
    SYSCALL_IS_SAFEA((void*)path,0);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    zeromem(first_path);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    zeromem(kpath);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    zeromem(result);
    vfs::resolve_path(kpath,first_path,result,1,0);
    
    int new_fd = vfs::fdmanager::create(proc);

    userspace_fd_t* new_fd_s = vfs::fdmanager::search(proc,new_fd);
    memcpy(new_fd_s->path,result,strlen(result));

    std::int32_t status = vfs::vfs::open(new_fd_s);
    if(status != 0)
        new_fd_s->state = USERSPACE_FD_STATE_UNUSED;
    
    return {1,status,status == 0 ? new_fd : -1};
}

syscall_ret_t sys_read(int fd, void *buf, size_t count) {
    SYSCALL_IS_SAFEA(buf,count);
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {1,EBADF,0};

    char* temp_buffer = (char*)memory::pmm::_virtual::alloc(count + 1);
    memory::paging::maprangeid(proc->original_cr3,Other::toPhys(temp_buffer),(std::uint64_t)temp_buffer,count,PTE_PRESENT | PTE_RW,proc->id);

    std::int64_t bytes_read;
    if(fd_s->state == USERSPACE_FD_STATE_FILE) 
        bytes_read = vfs::vfs::read(fd_s,temp_buffer,count);
    else if(fd_s->state == USERSPACE_FD_STATE_PIPE && fd_s->pipe) {
        bytes_read = fd_s->pipe->read(temp_buffer,count);
    } else
        return {1,EBADF,0};

    copy_in_userspace(proc,buf,temp_buffer,bytes_read);
    memory::pmm::_virtual::free(temp_buffer);

    return {1,bytes_read >= 0 ? 0 : (int)(+bytes_read), bytes_read};
}

syscall_ret_t sys_write(int fd, const void *buf, size_t count) {
    SYSCALL_IS_SAFEA((void*)buf,count);
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {1,EBADF,0};

    char* temp_buffer = (char*)memory::pmm::_virtual::alloc(count + 1);
    memory::paging::maprangeid(proc->original_cr3,Other::toPhys(temp_buffer),(std::uint64_t)temp_buffer,count,PTE_PRESENT | PTE_RW,proc->id);
    copy_in_userspace(proc,temp_buffer,(void*)buf,count);

    std::int64_t bytes_written;
    if(fd_s->state == USERSPACE_FD_STATE_FILE)
        bytes_written = vfs::vfs::write(fd_s,temp_buffer,count);
    else if(fd_s->state == USERSPACE_FD_STATE_PIPE) {
        bytes_written = fd_s->pipe->write(temp_buffer,count);
    } else
        return {1,EBADF,0};

    memory::pmm::_virtual::free(temp_buffer);
    return {1,bytes_written >= 0 ? 0 : (int)(+bytes_written), bytes_written};
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

syscall_ret_t sys_seek(int fd, long offset, int whence) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s) 
        return {1,EBADF,0};


    vfs::stat_t stat;
    vfs::vfs::stat(fd_s,&stat);

    switch (whence)
    {
        case SEEK_SET:
            fd_s->offset = offset;
            break;

        case SEEK_CUR:
            fd_s->offset += offset;
            break;

        case SEEK_END:
            fd_s->offset = stat.st_size + offset;
            break;

        default:
            return {1,EINVAL,0};
    }

    return {1,0,(std::int64_t)fd_s->offset};

}

syscall_ret_t sys_close(int fd) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return {0,EBADF,0};

    if(fd_s->state == USERSPACE_FD_STATE_PIPE)
        fd_s->pipe->close(fd_s->pipe_side);

    fd_s->state = USERSPACE_FD_STATE_UNUSED;

    return {0,0,0};
}

syscall_ret_t sys_stat(int fd, void* out) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return {0,EBADF,0};

    if(fd_s->state == USERSPACE_FD_STATE_FILE) {
        vfs::stat_t stat;
        int status = vfs::vfs::stat(fd_s,&stat);
        if(status != 0)
            return {0,status,0};
        copy_in_userspace(proc,out,&stat,sizeof(vfs::stat_t));
    } else {
        return {0,ENOENT,0};
    }
    return {0,0,0};
}

syscall_ret_t sys_pipe(int flags) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    int read_fd = vfs::fdmanager::create(proc);
    int write_fd = vfs::fdmanager::create(proc);
    userspace_fd_t* fd1 = vfs::fdmanager::search(proc,read_fd);
    userspace_fd_t* fd2 = vfs::fdmanager::search(proc,write_fd);

    vfs::pipe* new_pipe = new vfs::pipe(flags);
    fd1->pipe_side = PIPE_SIDE_READ;
    fd2->pipe_side = PIPE_SIDE_WRITE;

    fd1->pipe = new_pipe;
    fd2->pipe = new_pipe;

    fd1->state = USERSPACE_FD_STATE_PIPE;
    fd2->state = USERSPACE_FD_STATE_PIPE;

    return {1,read_fd,write_fd};

}