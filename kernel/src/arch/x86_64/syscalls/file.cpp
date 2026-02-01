
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <generic/mm/vmm.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>

#include <generic/vfs/fd.hpp>

#include <generic/vfs/devfs.hpp>

#include <drivers/tsc.hpp>

#include <etc/etc.hpp>
#include <etc/errno.hpp>

#include <etc/logging.hpp>

#include <arch/x86_64/syscalls/sockets.hpp>

#include <cstdint>

long long sys_access(char* path, int mode) {
    SYSCALL_IS_SAFEZ(path,4096);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    memset(first_path,0,2048);
    if(0)
        memcpy(first_path,vfs::fdmanager::search(proc,0)->path,strlen(vfs::fdmanager::search(proc,0)->path));
    else if(1 && proc->cwd)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    userspace_fd_t fd;
    memset(&fd,0,sizeof(userspace_fd_t));
    memcpy(fd.path,result,strlen(result) + 1);

    vfs::stat_t stat;
    int status = vfs::vfs::stat(&fd,&stat);
    return status;
}

long long sys_faccessat(int dirfd, char* path, int mode, int flags) {
    SYSCALL_IS_SAFEZ(path,4096);

    if(!path)
        return -EINVAL;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD && proc->cwd)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    userspace_fd_t fd;
    memset(&fd,0,sizeof(userspace_fd_t));
    memcpy(fd.path,result,strlen(result) + 1);

    vfs::stat_t stat;
    int status = 0;

    if(flags & AT_SYMLINK_NOFOLLOW) 
        status = vfs::vfs::nosym_stat(&fd,&stat);
    else
        status = vfs::vfs::stat(&fd,&stat);

    return status;
}

long long sys_openat(int dirfd, const char* path, int flags, int_frame_t* ctx) {
    SYSCALL_IS_SAFEZ((void*)path,0);
    if(!path)
        return -EFAULT;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    std::uint32_t mode = ctx->r10;

    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD && proc->cwd)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    DEBUG(proc->is_debug,"Trying to open %s, %s + %s from proc %d",result,first_path,path,proc->id);

    if(result[0] == '\0') {
        result[0] = '/';
        result[1] = '\0';
    }

    int new_fd = vfs::fdmanager::create(proc);

    userspace_fd_t* new_fd_s = vfs::fdmanager::search(proc,new_fd);
    memset(new_fd_s->path,0,2048);
    memcpy(new_fd_s->path,result,strlen(result));

    new_fd_s->offset = 0;
    new_fd_s->queue = 0;
    new_fd_s->pipe = 0;
    new_fd_s->pipe_side = 0;
    new_fd_s->cycle = 0;
    new_fd_s->is_a_tty = 0;
    new_fd_s->flags = flags; // copy flags

    if(flags & O_CREAT) 
        vfs::vfs::touch(new_fd_s->path, mode);

    new_fd_s->mode = mode;

    std::uint64_t start = drivers::tsc::currentnano();
    std::int32_t status = vfs::vfs::open(new_fd_s);

    if(status != 0) {
        new_fd_s->state = USERSPACE_FD_STATE_UNUSED;
        DEBUG(1,"Failed to open %s (%d:%s + %s) from proc %d, is_creat %d, is_trunc %d, flags %d (%s)",result,dirfd,first_path,path,proc->id,flags & O_CREAT,flags & O_TRUNC,flags,proc->name);
        return -status;
    }

    vfs::stat_t stat;
    std::int32_t stat_status = vfs::vfs::stat(new_fd_s,&stat);

    if(stat_status == 0) 
        if(flags & __O_DIRECTORY) 
            if(!(stat.st_mode & S_IFDIR)) {
                new_fd_s->state = USERSPACE_FD_STATE_UNUSED;
                return -ENOTDIR;
            }

    if(flags & O_TRUNC) 
        vfs::vfs::write(new_fd_s,(void*)"\0",1);

    if(flags & O_APPEND)
        new_fd_s->offset = stat.st_size;

    if(status != 0) {
        vfs::fdmanager* fdm = (vfs::fdmanager*)proc->fd;
        fdm->close(new_fd_s->index);
    }

    std::uint64_t end = drivers::tsc::currentnano();
    DEBUG(0,"open takes %llu time, status %d, %s",end - start,status,new_fd_s->path);

    if(status != 0)
        DEBUG(proc->is_debug,"Failed to open %s (%s + %s) from proc %d",result,first_path,path,proc->id);
    
    new_fd_s->offset = 0;

    DEBUG(proc->is_debug,"Open %s (%d) from proc %d",result,new_fd,proc->id);

    return status == 0 ? new_fd : -status;
}

typedef struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} __attribute__((packed)) stackframe_t;

long long sys_pread(int fd, void *buf, size_t count, int_frame_t* ctx) {

    std::uint64_t off = ctx->r10;
    SYSCALL_IS_SAFEZ(buf,count);
    
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_sz = vfs::fdmanager::search(proc,fd);
    if(!fd_sz)
        return -EBADF;

    userspace_fd_t fd_ss = *fd_sz;
    fd_ss.offset = off;

    userspace_fd_t* fd_s = &fd_ss;

    if(fd_s->can_be_closed)
        fd_s->can_be_closed = 0;

    DEBUG(proc->is_debug,"Trying to read %s (fd %d) with state %d from proc %d, pipe 0x%p (%s) count %lli",fd_s->path,fd,fd_s->state,proc->id,fd_s->pipe,proc->name,count);

    char* temp_buffer = (char*)buf;
    std::int64_t bytes_read;
    if(fd_s->state == USERSPACE_FD_STATE_FILE) 
        bytes_read = vfs::vfs::read(fd_s,temp_buffer,count);
    else if(fd_s->state == USERSPACE_FD_STATE_PIPE && fd_s->pipe) {
        if(fd_s->pipe_side != PIPE_SIDE_READ)
            return -EBADF;
        
        proc->debug1 = fd_s->pipe->lock.test();

        bytes_read = fd_s->pipe->read(&fd_s->read_counter,temp_buffer,count,(fd_s->flags & O_NONBLOCK) ? 1 : 0);

        if(bytes_read == -EAGAIN && (fd_s->pipe->flags & O_NONBLOCK || fd_s->flags & O_NONBLOCK) && !fd_s->pipe->is_closed.test()) 
            return -EAGAIN;

    } else if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {

        if(!fd_s->write_socket_pipe || !fd_s->read_socket_pipe)
            return -EFAULT;

        if(fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
            bytes_read = fd_s->write_socket_pipe->read(&fd_s->read_counter,temp_buffer,count,(fd_s->flags & O_NONBLOCK) ? 1 : 0);
        } else {
            bytes_read = fd_s->read_socket_pipe->read(&fd_s->read_counter,temp_buffer,count,(fd_s->flags & O_NONBLOCK) ? 1 : 0);
        }

        if(bytes_read == -EAGAIN)
            return -EAGAIN;

        if(bytes_read == 0)
            return 0;

    } else if(fd_s->state == USERSPACE_FD_STATE_EVENTFD) {
        if(count != 8)
            return -EINVAL;
        std::uint64_t* _8sizebuf = (std::uint64_t*)buf;
        bytes_read = fd_s->eventfd->read(_8sizebuf);
    } else
        return -EBADF;

    return bytes_read;
}

long long sys_read(int fd, void *buf, size_t count) {
    SYSCALL_IS_SAFEZ(buf,count);
    
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return -EBADF;

    if(fd_s->can_be_closed)
        fd_s->can_be_closed = 0;

    DEBUG(proc->is_debug,"Trying to read %s (fd %d) with state %d from proc %d, pipe 0x%p (%s) count %lli",fd_s->path,fd,fd_s->state,proc->id,fd_s->pipe,proc->name,count);

    char* temp_buffer = (char*)buf;
    std::int64_t bytes_read;
    if(fd_s->state == USERSPACE_FD_STATE_FILE) {
        bytes_read = vfs::vfs::read(fd_s,temp_buffer,count);
        
    }
    else if(fd_s->state == USERSPACE_FD_STATE_PIPE && fd_s->pipe) {
        if(fd_s->pipe_side != PIPE_SIDE_READ)
            return -EBADF;
        
        proc->debug1 = fd_s->pipe->lock.test();

        bytes_read = fd_s->pipe->read(&fd_s->read_counter,temp_buffer,count,(fd_s->flags & O_NONBLOCK) ? 1 : 0);

        if(bytes_read == -EAGAIN && (fd_s->pipe->flags & O_NONBLOCK || fd_s->flags & O_NONBLOCK) && !fd_s->pipe->is_closed.test()) 
            return -EAGAIN;

    } else if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {

        if(!fd_s->write_socket_pipe || !fd_s->read_socket_pipe)
            return -EFAULT;

        if(fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
            bytes_read = fd_s->write_socket_pipe->read(&fd_s->read_counter,temp_buffer,count,(fd_s->flags & O_NONBLOCK) ? 1 : 0);
        } else {
            bytes_read = fd_s->read_socket_pipe->read(&fd_s->read_counter,temp_buffer,count,(fd_s->flags & O_NONBLOCK) ? 1 : 0);
        }

        if(bytes_read == -EAGAIN)
            return -EAGAIN;

        if(bytes_read == 0)
            return 0;

    } else if(fd_s->state == USERSPACE_FD_STATE_EVENTFD) {
        if(count != 8)
            return -EINVAL;
        std::uint64_t* _8sizebuf = (std::uint64_t*)buf;
        bytes_read = fd_s->eventfd->read(_8sizebuf);
    } else
        return -EBADF;

    return bytes_read;
}

long long sys_writev(int fd, wiovec* vec, unsigned long vlen) {
    SYSCALL_IS_SAFEZ(vec,vlen * sizeof(wiovec));
     arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s) {
        DEBUG(1,"fail write fd %d from proc %d, proc->fd is 0x%p, %s\n",fd,proc->id,proc->fd,vec[0].iov_base);
        return -EBADF;
    }

    if(fd_s->can_be_closed)
        fd_s->can_be_closed = 0;

    std::uint64_t full_bytes_written = 0;

    for(unsigned long i = 0;i < vlen;i++) {
        wiovec cur_vec = vec[i];
        char* temp_buffer = (char*)cur_vec.iov_base;
        std::uint64_t count = cur_vec.iov_len;
        std::int64_t bytes_written;

        DEBUG(proc->is_debug,"Writing %s with content %s fd %d state %d from proc %d count %d writ sock 0x%p",fd_s->state == USERSPACE_FD_STATE_FILE ? fd_s->path : "Not file",temp_buffer,fd,fd_s->state,proc->id,count,fd_s->write_socket_pipe);

        if(fd_s->state == USERSPACE_FD_STATE_FILE)
            bytes_written = vfs::vfs::write(fd_s,temp_buffer,count);
        else if(fd_s->state == USERSPACE_FD_STATE_PIPE) {
            if(fd_s->pipe_side != PIPE_SIDE_WRITE)
                return -EBADF;
            bytes_written = fd_s->pipe->write(temp_buffer,count,proc->id);
        } else if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {

            if(!fd_s->write_socket_pipe || !fd_s->read_socket_pipe)
                return -EFAULT;
            
            if(fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
                bytes_written = fd_s->read_socket_pipe->write(temp_buffer,count,proc->id);
            } else {
                bytes_written = fd_s->write_socket_pipe->write(temp_buffer,count,proc->id);
            }

            if(bytes_written == 0)
                return 0;

        } else if(fd_s->state == USERSPACE_FD_STATE_EVENTFD) {
            if(count != 8)
                return -EINVAL;
            std::uint64_t* _8sizebuf = (std::uint64_t*)temp_buffer;
            bytes_written = fd_s->eventfd->write(*_8sizebuf);
        } else
            return -EBADF;
        full_bytes_written += bytes_written;
    }
    
    return full_bytes_written;
}

long long sys_write(int fd, const void *buf, size_t count) {
    SYSCALL_IS_SAFEZ((void*)buf,count);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s) {
        DEBUG(proc->is_debug,"fail write fd %d from proc %d, proc->fd is 0x%p\n",fd,proc->id,proc->fd);
        return -EBADF;
    }

    if(fd_s->can_be_closed)
        fd_s->can_be_closed = 0;

    char* temp_buffer = (char*)buf;   

    const char* _0 = "Content view is disabled in files";
    DEBUG(proc->is_debug,"Writing %s with content %s fd %d state %d from proc %d count %d writ sock 0x%p",fd_s->state == USERSPACE_FD_STATE_FILE ? fd_s->path : "Not file",buf,fd,fd_s->state,proc->id,count,fd_s->write_socket_pipe);

    if(fd == 1) {
        //Log::SerialDisplay(LEVEL_MESSAGE_INFO,"%s\n",buf);
    }

    std::int64_t bytes_written;
    if(fd_s->state == USERSPACE_FD_STATE_FILE)
        bytes_written = vfs::vfs::write(fd_s,temp_buffer,count);
    else if(fd_s->state == USERSPACE_FD_STATE_PIPE) {
        if(fd_s->pipe_side != PIPE_SIDE_WRITE)
            return -EBADF;
        bytes_written = fd_s->pipe->write(temp_buffer,count,proc->id);
    } else if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {

        if(!fd_s->write_socket_pipe || !fd_s->read_socket_pipe)
            return -EFAULT;
        
        if(fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
            bytes_written = fd_s->read_socket_pipe->write(temp_buffer,count,proc->id);
        } else {
            bytes_written = fd_s->write_socket_pipe->write(temp_buffer,count,proc->id);
        }

        if(bytes_written == 0)
            return 0;

    } else if(fd_s->state == USERSPACE_FD_STATE_EVENTFD) {
        if(count != 8)
            return -EINVAL;
        std::uint64_t* _8sizebuf = (std::uint64_t*)buf;
        bytes_written = fd_s->eventfd->write(*_8sizebuf);
    } else
        return -EBADF;
        

    return bytes_written;
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

long long sys_sendto(int s, void* msg, size_t len) {
    return sys_write(s,msg,len);
}

long long sys_lseek(int fd, long offset, int whence) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s) 
        return -EBADF;

    if(fd_s->state == USERSPACE_FD_STATE_PIPE || fd_s->is_a_tty)
        return 0;

    switch (whence)
    {
        case SEEK_SET:
            fd_s->offset = offset;
            break;

        case SEEK_CUR:
            fd_s->offset += offset;
            break;

        case SEEK_END: {
            vfs::stat_t stat;
            int res = vfs::vfs::stat(fd_s,&stat);

            if(res != 0) {
                return -res;
            }

            fd_s->offset = stat.st_size + offset;
            break;
        }

        default:
            return -EINVAL;
    }

    return fd_s->offset;

}

long long sys_close(int fd) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s && fd > 2)
        return -EBADF;
    else if(fd < 3 && !fd_s)
        return 0; // ignoring

    if(fd_s->state != USERSPACE_FD_STATE_SOCKET) {
        DEBUG(proc->is_debug,"closing %d from proc %d pipe 0x%p",fd,proc->id,fd_s->pipe);
    } else 
        DEBUG(proc->is_debug,"closing unix socket %d connected to proc %d from proc %d",fd,fd_s->socket_pid,proc->id);

    if(fd_s->can_be_closed)
        return -EBADF;

    proc->fd_lock.lock();

    if(fd < 3)
        fd_s->can_be_closed = 1;

    if(fd_s->state == USERSPACE_FD_STATE_PIPE) {
        fd_s->pipe->close(fd_s->pipe_side);
    } else if(fd_s->state == USERSPACE_FD_STATE_FILE || fd_s->state == USERSPACE_FD_STATE_SOCKET)
        vfs::vfs::close(fd_s); 
    else if(fd_s->state == USERSPACE_FD_STATE_EVENTFD)
        fd_s->eventfd->close();

    if(!fd_s->is_a_tty && fd_s->index > 2)
        fd_s->state = USERSPACE_FD_STATE_UNUSED;

    proc->fd_lock.unlock();

    return 0;
}

long long sys_fstat(int fd, void* out) {
    int flags = 0;
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    memset(out,0,sizeof(vfs::stat_t));

    if(!fd_s)
        return -EBADF;

    if(!out)
        return -EBADF;

    DEBUG(proc->is_debug,"Trying to fstat %s (fd %d) from proc %d",fd_s->path,fd,proc->id);

    if(fd_s->state == USERSPACE_FD_STATE_FILE) {

        vfs::stat_t stat;
        int status;
        if(flags & AT_SYMLINK_NOFOLLOW) {
            status = vfs::vfs::nosym_stat(fd_s,&stat); 
        }
        else 
            status = vfs::vfs::stat(fd_s,&stat);
        if(status != 0)
            return -status;
        copy_in_userspace(proc,out,&stat,sizeof(vfs::stat_t));
    } else {
        vfs::stat_t stat;
        memset(&stat,0,sizeof(vfs::stat_t));
        if(fd_s->state == USERSPACE_FD_STATE_SOCKET)
            stat.st_mode |= S_IFSOCK;
        else if(fd_s->state == USERSPACE_FD_STATE_PIPE)
            stat.st_mode |= S_IFIFO;
        copy_in_userspace(proc,out,&stat,sizeof(vfs::stat_t));
        return 0;
    }
    //DEBUG(1,"ret ino %d",((vfs::stat_t*)out)->st_ino);
    return 0;
}


long long sys_newfstatat(int fd, char* path, void* out, int_frame_t* ctx) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_sz = vfs::fdmanager::search(proc,fd);
    userspace_fd_t fd_tt;
    userspace_fd_t* fd_s = &fd_tt;

    memset(out,0,sizeof(vfs::stat_t));

    int flags = ctx->r10;

    if(!fd_sz && fd != -100)
        return -EBADF;

    if(flags & AT_EMPTY_PATH && !path) {
        if(!fd_sz)
            return -EBADF;
        fd_s = fd_sz;
    } else {

        if(!path)
            return -EINVAL;

        char first_path[2048];
        memset(first_path,0,2048);
        if(fd >= 0)
            memcpy(first_path,vfs::fdmanager::search(proc,fd)->path,strlen(vfs::fdmanager::search(proc,fd)->path));
        else if(fd == AT_FDCWD && proc->cwd)
            memcpy(first_path,proc->cwd,strlen(proc->cwd));

        char kpath[2048];
        memset(kpath,0,2048);
        copy_in_userspace_string(proc,kpath,(void*)path,2048);

        char result[2048];
        memset(result,0,2048);
        vfs::resolve_path(kpath,first_path,result,1,0);
        memset(&fd_tt,0,sizeof(userspace_fd_t));
        memcpy(fd_tt.path,result,strlen(result) + 1);
        fd_tt.state = USERSPACE_FD_STATE_FILE;
    }

    if(!out)
        return -EINVAL;

    DEBUG(proc->is_debug,"Trying to newfstatat %s (fd %d), state %d from proc %d",fd_s->path,fd,proc->id);

    if(fd_s->state == USERSPACE_FD_STATE_FILE) {

        vfs::stat_t stat;
        int status;
        if(flags & AT_SYMLINK_NOFOLLOW) {
            status = vfs::vfs::nosym_stat(fd_s,&stat); 
        }
        else 
            status = vfs::vfs::stat(fd_s,&stat);
        //DEBUG(1,"status %d\n",status);
        if(status != 0)
            return -status;
        copy_in_userspace(proc,out,&stat,sizeof(vfs::stat_t));
    } else {
        vfs::stat_t stat;
        memset(&stat,0,sizeof(vfs::stat_t));
        if(fd_s->state == USERSPACE_FD_STATE_SOCKET)
            stat.st_mode |= S_IFSOCK;
        else if(fd_s->state == USERSPACE_FD_STATE_PIPE)
            stat.st_mode |= S_IFIFO;
        copy_in_userspace(proc,out,&stat,sizeof(vfs::stat_t));
        return 0;
    }
    //DEBUG(1,"ret ino %d",((vfs::stat_t*)out)->st_ino);
    return 0;
}

syscall_ret_t sys_pathstat(char* path, void* out, int flags, int_frame_t* ctx) {
    SYSCALL_IS_SAFEA(path,4096);
    if(!path)
        return {0,EINVAL,0};

    if(!out)
        return {0,EINVAL,0};

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    memset(first_path,0,2048);
    memcpy(first_path,proc->cwd,strlen(proc->cwd));
        
    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    if(result[0] == '\0') {
        result[0] = '/';
        result[1] = '\0';
    }

    userspace_fd_t fd_s;
    memcpy(fd_s.path,result,strlen(result) + 1);
    fd_s.is_cached_path = 0;

    DEBUG(proc->is_debug,"Trying to stat %s from proc %d",result,proc->id);

    vfs::stat_t stat;

    memset(&stat,0,sizeof(vfs::stat_t));

    int status;
    if(flags & AT_SYMLINK_NOFOLLOW) {
        status = vfs::vfs::nosym_stat(&fd_s,&stat); 
    }
    else 
        status = vfs::vfs::stat(&fd_s,&stat);
    if(status != 0)
        return {0,status,0};
    copy_in_userspace(proc,out,&stat,sizeof(vfs::stat_t));

    return {0,0,0};
}

long long sys_pipe(int* fildes) {
    return sys_pipe2(fildes,0);
}

long long sys_pipe2(int* fildes, int flags) {

    if(!fildes)
        return -EINVAL;

    SYSCALL_IS_SAFEZ(fildes,4096);

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

    fd1->is_cloexec = (flags & __O_CLOEXEC) ? 1 : 0;
    fd2->is_cloexec = (flags & __O_CLOEXEC) ? 1 : 0;

    DEBUG(proc->is_debug,"Creating pipe %d-%d from proc %d, cloexec: %d (flags %d)",read_fd,write_fd,proc->id,flags & __O_CLOEXEC,flags);

    fildes[0] = read_fd;
    fildes[1] = write_fd;

    return 0;
}

long long sys_dup(int fd) {
    int flags = 0;
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    
    if(!fd_s)
        return -EBADF;
    
    proc->fd_lock.lock();

    int i = 0;
    userspace_fd_t* nfd_s = vfs::fdmanager::searchlowest(proc,0);

    if(!nfd_s) {
        proc->fd_lock.unlock();
        int new_fd = vfs::fdmanager::create(proc);
        nfd_s = vfs::fdmanager::search(proc,new_fd);
        i = 1;
    }

    DEBUG(proc->is_debug,"dup from %d to %d",fd,nfd_s->index);

    nfd_s->cycle = fd_s->cycle;
    nfd_s->offset = fd_s->offset;
    nfd_s->state = fd_s->state;
    nfd_s->other_state = fd_s->other_state;
    nfd_s->pipe = fd_s->pipe;
    nfd_s->pipe_side = fd_s->pipe_side;
    nfd_s->queue = fd_s->queue;
    nfd_s->is_a_tty = fd_s->is_a_tty;
    nfd_s->read_counter = fd_s->read_counter;
    nfd_s->write_counter = fd_s->write_counter;
    nfd_s->can_be_closed = 0;
    nfd_s->write_socket_pipe = fd_s->write_socket_pipe;
    nfd_s->read_socket_pipe = fd_s->read_socket_pipe;
    nfd_s->eventfd = fd_s->eventfd;
    nfd_s->flags = fd_s->flags;

    memcpy(nfd_s->path,fd_s->path,sizeof(fd_s->path));

    if(nfd_s->state == USERSPACE_FD_STATE_PIPE)
        nfd_s->pipe->create(nfd_s->pipe_side);
    else if(nfd_s->state == USERSPACE_FD_STATE_EVENTFD)
        nfd_s->eventfd->create();

    if(i == 0)
        proc->fd_lock.unlock();

    return nfd_s->index;
}

long long sys_dup2(int fd, int newfd) {
    int flags = 0;
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return -EBADF;

    userspace_fd_t* nfd_s = vfs::fdmanager::search(proc,newfd);
    if(!nfd_s) {
        int t = vfs::fdmanager::create(proc);
        nfd_s = vfs::fdmanager::search(proc,t);
    } else {
        if(nfd_s->state == USERSPACE_FD_STATE_PIPE) 
            nfd_s->pipe->close(nfd_s->pipe_side);
        else if(nfd_s->state == USERSPACE_FD_STATE_EVENTFD)
            nfd_s->eventfd->close();
    }

    nfd_s->index = newfd;
    nfd_s->cycle = fd_s->cycle;
    nfd_s->offset = fd_s->offset;
    nfd_s->state = fd_s->state;
    nfd_s->other_state = fd_s->other_state;
    nfd_s->pipe = fd_s->pipe;
    nfd_s->pipe_side = fd_s->pipe_side;
    nfd_s->queue = fd_s->queue;
    nfd_s->is_a_tty = fd_s->is_a_tty;
    nfd_s->write_counter = fd_s->write_counter;
    nfd_s->read_counter = fd_s->read_counter;
    nfd_s->can_be_closed = fd_s->can_be_closed;
    nfd_s->read_socket_pipe = fd_s->read_socket_pipe;
    nfd_s->write_socket_pipe = fd_s->write_socket_pipe;
    nfd_s->eventfd = fd_s->eventfd;
    nfd_s->flags = fd_s->flags;
    nfd_s->is_cloexec = (flags & __O_CLOEXEC) ? 1 : 0;

    if(nfd_s->state == USERSPACE_FD_STATE_PIPE)
        nfd_s->pipe->create(nfd_s->pipe_side);
    else if(nfd_s->state == USERSPACE_FD_STATE_EVENTFD)
        nfd_s->eventfd->create();

    DEBUG(proc->is_debug,"dup2 from %d to %d from proc %d\n",fd,newfd,proc->id);

    memcpy(nfd_s->path,fd_s->path,sizeof(fd_s->path));

    return nfd_s->index;
}

syscall_ret_t sys_create_dev(std::uint64_t requestandnum, char* slave_path, char* master_path) {
    char slave[256];
    char master[256];
    memset(slave,0,sizeof(slave));
    memset(master,0,sizeof(master));

    arch::x86_64::process_t* proc = CURRENT_PROC;

    copy_in_userspace_string(proc,slave,slave_path,256);
    copy_in_userspace_string(proc,master,master_path,256);

    vfs::devfs_packet_t packet;
    packet.size = requestandnum & 0xFFFFFFFF;
    packet.request = (uint32_t)(requestandnum >> 32);
    packet.value = (std::uint64_t)&master[0];

    vfs::devfs::send_packet(slave,&packet);
    return {0,0,0};
}

long long sys_ioctl(int fd, unsigned long request, void *arg) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return -EBADF;

    DEBUG(proc->is_debug,"ioctl fd %d (%s) req 0x%p arg 0x%p\n",fd,fd_s->path,request,arg);

    int res = 0;

    SYSCALL_IS_SAFEZ((void*)arg,4096);
 
    std::int64_t ret = vfs::vfs::ioctl(fd_s,request,(void*)arg,&res);

    return ret != 0 ? -ret : 0;
}

syscall_ret_t sys_create_ioctl(char* path, std::uint64_t write_and_read_req, std::uint32_t size) {
    
    std::int32_t read_req = write_and_read_req & 0xFFFFFFFF;
    std::int32_t write_req = (uint32_t)(write_and_read_req >> 32);

    char path0[256];
    memset(path0,0,256);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    copy_in_userspace_string(proc,path0,path,256);

    vfs::devfs_packet_t packet;
    packet.create_ioctl.readreg = read_req;
    packet.create_ioctl.writereg = write_req;
    packet.create_ioctl.request = DEVFS_PACKET_CREATE_IOCTL;
    packet.create_ioctl.size = size;
    packet.create_ioctl.pointer = (std::uint64_t)memory::pmm::_virtual::alloc(size);
    vfs::devfs::send_packet(path0,&packet);

    return {0,0,0};
}

syscall_ret_t sys_setup_tty(char* path) {

    char path0[256];
    memset(path0,0,256);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    copy_in_userspace_string(proc,path0,path,256);

    vfs::devfs_packet_t packet;
    packet.request = DEVFS_PACKET_SETUPTTY;
    vfs::devfs::send_packet(path0,&packet);

    return {0,0,0};
}

syscall_ret_t sys_isatty(int fd) {

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {0,EBADF,0};

    if(fd_s->state == USERSPACE_FD_STATE_PIPE || fd_s->state == USERSPACE_FD_STATE_SOCKET)
        return {0,ENOTTY,0};

    int ret = vfs::vfs::var(fd_s,0,DEVFS_VAR_ISATTY);

    DEBUG(proc->is_debug,"sys_isatty %s fd %d ret %d",fd_s->path,fd,ret);

    fd_s->is_a_tty = ret == 0 ? 1 : 0;

    return {0,ret != 0 ? ENOTTY : 0,0};
}

syscall_ret_t sys_setupmmap(char* path, std::uint64_t addr, std::uint64_t size, int_frame_t* ctx) {
    std::uint64_t flags = ctx->r8;

    char path0[256];
    memset(path0,0,256);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    copy_in_userspace_string(proc,path0,path,256);

    vfs::devfs_packet_t packet;
    packet.request = DEVFS_SETUP_MMAP;
    packet.setup_mmap.dma_addr = addr;
    packet.setup_mmap.size = size;
    packet.setup_mmap.flags = flags;

    vfs::devfs::send_packet(path0,&packet);

    return {0,0,0};

}

syscall_ret_t sys_ptsname(int fd, void* out, int max_size) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {0,EBADF,0};

    char buffer[256];
    memset(buffer,0,256);

    vfs::devfs_packet_t packet;
    packet.value = (std::uint64_t)buffer;
    packet.request = DEVFS_GETSLAVE_BY_MASTER;

    vfs::devfs::send_packet(fd_s->path + 4,&packet);
    
    if(strlen(buffer) >= max_size)
        return {0,ERANGE,0};
    
    char buffer2[256];
    memset(buffer2,0,256);

    __printfbuf(buffer2,256,"/dev%s",buffer);
    zero_in_userspace(proc,out,max_size);
    copy_in_userspace(proc,out,buffer2,strlen(buffer2));

    return {0,0,0};
}

syscall_ret_t sys_setup_ring_bytelen(char* path, int bytelen) {
    char path0[256];
    memset(path0,0,256);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    copy_in_userspace_string(proc,path0,path,256);

    vfs::devfs_packet_t packet;
    packet.request = DEVFS_PACKET_SETUP_RING_SIZE;
    packet.size = bytelen;

    vfs::devfs::send_packet(path0,&packet);

    return {0,0,0};
}

syscall_ret_t sys_read_dir(int fd, void* buffer) {

    SYSCALL_IS_SAFEA(buffer,4096);
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {0,EBADF,0};

    vfs::dirent_t dirent;
    memset(&dirent,0,sizeof(vfs::dirent_t));

    int status = vfs::vfs::ls(fd_s,&dirent);
        
    copy_in_userspace(proc,buffer,&dirent,sizeof(vfs::dirent_t));
    return {1,status,dirent.d_reclen};
    
}

long long __fcntl_dup_fd(int fd, int is_cloexec, int lowest, int_frame_t* ctx) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return -EBADF;
    
    proc->fd_lock.lock();

    int i = 0;
    userspace_fd_t* nfd_s = vfs::fdmanager::searchlowestfrom(proc,lowest);

    if(!nfd_s) {
        proc->fd_lock.unlock();
        int new_fd = vfs::fdmanager::create(proc);
        nfd_s = vfs::fdmanager::search(proc,new_fd);
        i = 1;
    }

    DEBUG(proc->is_debug,"dup from %d to %d",fd,nfd_s->index);

    nfd_s->cycle = fd_s->cycle;
    nfd_s->offset = fd_s->offset;
    nfd_s->state = fd_s->state;
    nfd_s->other_state = fd_s->other_state;
    nfd_s->pipe = fd_s->pipe;
    nfd_s->pipe_side = fd_s->pipe_side;
    nfd_s->queue = fd_s->queue;
    nfd_s->is_a_tty = fd_s->is_a_tty;
    nfd_s->read_counter = fd_s->read_counter;
    nfd_s->write_counter = fd_s->write_counter;
    nfd_s->can_be_closed = 0;
    nfd_s->write_socket_pipe = fd_s->write_socket_pipe;
    nfd_s->read_socket_pipe = fd_s->read_socket_pipe;
    nfd_s->eventfd = fd_s->eventfd;
    nfd_s->flags = fd_s->flags;

    memcpy(nfd_s->path,fd_s->path,sizeof(fd_s->path));

    if(nfd_s->state == USERSPACE_FD_STATE_PIPE)
        nfd_s->pipe->create(nfd_s->pipe_side);
    else if(nfd_s->state == USERSPACE_FD_STATE_EVENTFD)
        nfd_s->eventfd->create();

    if(i == 0)
        proc->fd_lock.unlock();

    return nfd_s->index;
}

#define F_DUPFD_CLOEXEC 1030
#define F_GETFD  1
#define F_SETFD  2

long long sys_fcntl(int fd, int request, std::uint64_t arg, int_frame_t* ctx) {

    arch::x86_64::process_t* pproc = CURRENT_PROC;

    DEBUG(pproc->is_debug,"fcntl fd %d req %d arg 0x%p from proc %d",fd,request,arg,pproc->id);
    int is_cloexec = 0;
    switch(request) {
        case F_DUPFD_CLOEXEC:
            is_cloexec = 1;
        case F_DUPFD: {
            long long r = __fcntl_dup_fd(fd,is_cloexec,arg,ctx);
            DEBUG(pproc->is_debug,"return fd %d from dup fcntl from proc %d",r,pproc->id);
            return r; 
        }

        case F_GETFD: {
            arch::x86_64::process_t* proc = CURRENT_PROC;
            userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
            if(!fd_s)
                return -EBADF;
            return fd_s->is_cloexec;
        }

        case F_SETFD: {
            arch::x86_64::process_t* proc = CURRENT_PROC;
            userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
            if(!fd_s)
                return -EBADF;

            fd_s->is_cloexec = arg & 1;
            return 0;
        }

        case F_GETFL: {
            arch::x86_64::process_t* proc = CURRENT_PROC;
            userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
            if(!fd_s)
                return -EBADF;

            return (fd_s->flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
        }

        case F_SETFL: {
            arch::x86_64::process_t* proc = CURRENT_PROC;
            userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

            if(!fd_s)
                return -EBADF;

            fd_s->flags &= ~(O_APPEND | O_ASYNC | O_NONBLOCK | O_RDONLY | O_RDWR | O_WRONLY);
            fd_s->flags |= (arg & (O_APPEND | O_ASYNC | O_NONBLOCK | O_RDONLY | O_RDWR | O_WRONLY));
            if(fd_s->state == USERSPACE_FD_STATE_EVENTFD) {
                fd_s->eventfd->flags &= ~(O_NONBLOCK);
                fd_s->eventfd->flags |= (arg & O_NONBLOCK);
            }

            return 0;
        }

        default: {
            Log::SerialDisplay(LEVEL_MESSAGE_WARN,"Unsupported fcntl %d arg 0x%p to fd %d from proc %d\n",request,arg,fd,pproc->id);
            return 0;
        }
    }
    return -EINVAL;
}

long long sys_chdir(char* path) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    SYSCALL_IS_SAFEZ(path,4096);

    if(!path)
        return -EINVAL;

    int dirfd = AT_FDCWD;

    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    userspace_fd_t fd_s;
    memset(&fd_s,0,sizeof(userspace_fd_t));
    memcpy(fd_s.path,result,strlen(result) + 1);

    vfs::stat_t stat;
    int ret = vfs::vfs::stat(&fd_s,&stat);

    if(ret != 0)
        return -ret;

    if(!(stat.st_mode & S_IFDIR))
        return -ENOTDIR;

    memcpy(proc->cwd,result,strlen(result) + 1);
    return 0;
}

long long sys_fchdir(int fd) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return -EBADF;

    vfs::stat_t stat;
    int ret = vfs::vfs::stat(fd_s,&stat);

    if(ret != 0)
        return -ret;

    if(!(stat.st_mode & S_IFDIR))
        return -ENOTDIR;

    memset(proc->cwd,0,4096);
    memcpy(proc->cwd,fd_s->path,2048);

    return 0;

}

syscall_ret_t sys_mkfifoat(int dirfd, const char *path, int mode) {
    SYSCALL_IS_SAFEA((void*)path,0);

    arch::x86_64::process_t* proc = CURRENT_PROC;
    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    userspace_fd_t new_fd_s;
    new_fd_s.is_cached_path = 0;
    memset(new_fd_s.path,0,2048);
    memcpy(new_fd_s.path,result,strlen(result));

    vfs::stat_t stat;

    if(vfs::vfs::stat(&new_fd_s,&stat) == 0)
        return {0,EEXIST,0};

    int status = vfs::vfs::create_fifo(result);


    return {0,status,0};
}

void poll_to_str(int event, char* out) {
    const char* result = "Undefined";
    if(event & POLLIN && event & POLLOUT) {
        result = "POLLIN and POLLOUT";
    } else if(event & POLLIN) {
        result = "POLLIN";
    } else if(event & POLLOUT) {
        result = "POLLOUT";
    }
    memcpy(out,result,strlen(result) + 1);
}

long long sys_poll(struct pollfd *fds, int count, int timeout, int_frame_t* ctx) {
    if(ctx->rax != 270) { // ban returning efault from pselect
        SYSCALL_IS_SAFEZ((void*)fds,0);
    }

    if(!fds)
        return -EINVAL;

    arch::x86_64::process_t* proc = CURRENT_PROC;
    proc->debug0 = count;
    proc->debug1 = timeout;

    struct pollfd* fd = fds;

    int total_events = 0;

    userspace_fd_t* cached_fds[count];

    for(int i = 0;i < count; i++) {
        fd[i].revents = 0; 
    }

    for(int i = 0;i < count; i++) {
        userspace_fd_t* fd0 = vfs::fdmanager::search(proc,fd[i].fd);

        cached_fds[i] = fd0;

        fd[i].revents = 0;
        

        if(!fd0) { return -EBADF;}

        if(fd[i].events & POLLIN)
            total_events++;

        if(fd[i].events & POLLOUT) {
            
            total_events++; 
        }

        char out[64];
        if(proc->is_debug) {
            poll_to_str(fd[i].events,out);

            DEBUG(1,"Trying to poll %s (%d) timeout %d event %s with state %d is_listen %d from proc %d (%s)",fd0->state == USERSPACE_FD_STATE_FILE ? fd0->path : "Not file",fd0->index,timeout,out,fd0->state,fd0->is_listen,proc->id,proc->name);
         
        }   
    }

    int num_events = 0;

    int success = true;
    int something_bad = 0;

    int retry = 0;

    int is_first = 1;

    if(timeout == -1) {
        while(success) {

            for(int i = 0;i < count; i++) {
                userspace_fd_t* fd0 = cached_fds[i];
                if(fd[i].events & POLLIN) {

                    if(fd0->state == USERSPACE_FD_STATE_SOCKET && !fd0->is_listen && fd0->write_socket_pipe && fd0->read_socket_pipe) {
                        int if_pollin = 0;

                        if(fd0->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
                            if_pollin = fd0->write_socket_pipe->size.load() > 0 ? 1 : 0;
                        } else {
                            if_pollin = fd0->read_socket_pipe->size.load() > 0 ? 1 : 0;
                        }
                        if(if_pollin) {
                            //DEBUG(fd0->index == 18 || fd0->index == 19 || fd0->index == 20,"polin done fd %d writ sock 0x%p pip 0x%p",fd0->index,fd0->write_socket_pipe,fd0->pipe);
                            num_events++;
                            fd[i].revents |= POLLIN;
                        }

                        if(fd0->write_socket_pipe->is_closed.test() && !if_pollin) {
                            num_events++;
                            fd[i].revents |= POLLHUP;
                        }
                    } else if(fd0->state == USERSPACE_FD_STATE_SOCKET && fd0->is_listen && fd0->binded_socket) {
                        if(fd0->read_counter == -1)
                            fd0->read_counter = 0;
                        socket_node_t* nod = (socket_node_t*)fd0->binded_socket;
                        if(nod->socket_counter > (std::uint64_t)fd0->read_counter) {
                            fd0->read_counter++;
                            num_events++;
                            fd[i].revents |= POLLIN;
                        }
                    } else if(fd0->state == USERSPACE_FD_STATE_PIPE) {
                        if(fd0->pipe_side != PIPE_SIDE_READ)
                            continue;
                        if(fd0->pipe->size.load() > 0) {
                            num_events++;
                            fd[i].revents |= POLLIN; 
                        }
                    } else if(fd0->state == USERSPACE_FD_STATE_EVENTFD) {
                        fd0->eventfd->lock.lock();
                        if(fd0->eventfd->buffer > 0) {
                            num_events++;
                            fd[i].revents |= POLLIN; 
                        }
                        fd0->eventfd->lock.unlock();
                    } else {

                        std::int64_t ret = vfs::vfs::poll(fd0,POLLIN);
                        if(ret != 0) {
                            num_events++;
                            fd[i].revents |= POLLIN;
                        }
                    }
                }

                if(fd[i].events & POLLOUT) {
                    if(fd0->state == USERSPACE_FD_STATE_SOCKET && fd0->write_socket_pipe && fd0->read_socket_pipe && !fd0->is_listen) {
                        int is_pollout = 0;
                        if(fd0->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
                            if(fd0->read_socket_pipe->size.load() < fd0->read_socket_pipe->total_size)
                                is_pollout = 1;
                        } else {
                            if(fd0->write_socket_pipe->size.load() < fd0->write_socket_pipe->total_size)
                                is_pollout = 1;
                        }

                        if(is_pollout) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                    } else if(fd0->state == USERSPACE_FD_STATE_EVENTFD) {
                        fd0->eventfd->lock.lock();
                        if(fd0->eventfd->buffer <= (0xFFFFFFFFFFFFFFFF - 1)) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                        fd0->eventfd->lock.unlock();
                    } else if(fd0->state == USERSPACE_FD_STATE_PIPE) {
                        if(fd0->pipe_side != PIPE_SIDE_READ)
                            continue;
                        if(fd0->pipe->size.load() < fd0->pipe->total_size) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                    } else {
                        std::int64_t ret = vfs::vfs::poll(fd0,POLLOUT);
                            
                        if(ret != 0) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                    }

                }



                if(num_events)
                    success = false;
            }

            if(success)
                yield();
            asm volatile("pause");
        }
    } else {
        int tries = 0;
        std::uint64_t current_timestamp = drivers::tsc::currentus();
        std::uint64_t end_timestamp = (current_timestamp + (timeout * 1000));
        while(drivers::tsc::currentus() < end_timestamp && success) {
            for(int i = 0;i < count; i++) {
                userspace_fd_t* fd0 = cached_fds[i];
                if(fd[i].events & POLLIN) {
                    //std::int64_t ret = vfs::vfs::poll(fd0,POLLIN);
                    if(fd[i].events & POLLIN) {

                    if(fd0->state == USERSPACE_FD_STATE_SOCKET && !fd0->is_listen && fd0->write_socket_pipe && fd0->read_socket_pipe) {
                        int if_pollin = 0;
                        
                        if(fd0->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
                            if_pollin = fd0->write_socket_pipe->size.load() > 0 ? 1 : 0;
                        } else {
                            if_pollin = fd0->read_socket_pipe->size.load() > 0 ? 1 : 0;
                        }
                        if(if_pollin) {
                            //DEBUG(fd0->index == 18 || fd0->index == 19 || fd0->index == 20,"polin done fd %d writ sock 0x%p pip 0x%p",fd0->index,fd0->write_socket_pipe,fd0->pipe);
                            num_events++;
                            fd[i].revents |= POLLIN;
                        }

                        if(fd0->write_socket_pipe->is_closed.test() && !if_pollin) {
                            num_events++;
                            fd[i].revents |= POLLHUP;
                        }

                    } else if(fd0->state == USERSPACE_FD_STATE_SOCKET && fd0->is_listen && fd0->binded_socket) {
                        if(fd0->read_counter == -1)
                            fd0->read_counter = 0;

                        socket_node_t* nod = (socket_node_t*)fd0->binded_socket;
                        if(nod->socket_counter > (std::uint64_t)fd0->read_counter) {
                            fd0->read_counter++;
                            num_events++;
                            fd[i].revents |= POLLIN;
                        }
                    } else if(fd0->state == USERSPACE_FD_STATE_PIPE) {
                        if(fd0->pipe->size.load() > 0) {
                            num_events++;
                            fd[i].revents |= POLLIN; 
                        }
                    } else if(fd0->state == USERSPACE_FD_STATE_EVENTFD) {
                        fd0->eventfd->lock.lock();
                        if(fd0->eventfd->buffer > 0) {
                            num_events++;
                            fd[i].revents |= POLLIN; 
                        }
                        fd0->eventfd->lock.unlock();
                    } else {

                        std::int64_t ret = vfs::vfs::poll(fd0,POLLIN);
                            
                        if(ret != 0) {
                            num_events++;
                            fd[i].revents |= POLLIN;
                        }
                    }
                    }

                }

                if(fd[i].events & POLLOUT) {
                    if(fd0->state == USERSPACE_FD_STATE_SOCKET) {
                        int is_pollout = 0;
                        if(fd0->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
                            if(fd0->read_socket_pipe->size.load() < fd0->read_socket_pipe->total_size)
                                is_pollout = 1;
                        } else {
                            if(fd0->write_socket_pipe->size.load() < fd0->write_socket_pipe->total_size)
                                is_pollout = 1;
                        }

                        if(is_pollout) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                    } else if(fd0->state == USERSPACE_FD_STATE_EVENTFD) {
                        fd0->eventfd->lock.lock();
                        if(fd0->eventfd->buffer <= (0xFFFFFFFFFFFFFFFF - 1)) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                        fd0->eventfd->lock.unlock();
                    } else if(fd0->state == USERSPACE_FD_STATE_PIPE) {
                        if(fd0->pipe->size < fd0->pipe->total_size) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                    } else {
                        std::int64_t ret = vfs::vfs::poll(fd0,POLLOUT);
                        if(ret != 0) {
                            num_events++;
                            fd[i].revents |= POLLOUT;
                        }
                    }

                }

                if(num_events)
                    success = false;
                // w
            }

            if(timeout == 0)
                break;

            std::uint64_t start = drivers::tsc::currentus();
            if(drivers::tsc::currentus() < end_timestamp && success) {
                yield(); 
                std::uint64_t now = drivers::tsc::currentus();
            }
            
        }
    }

    DEBUG(proc->is_debug,"%d from proc %d num_ev %d",timeout,proc->id,num_events);
    //copy_in_userspace(proc,fds,fd,count * sizeof(struct pollfd));
    return num_events;


}

long long sys_pselect6(int num_fds, fd_set* read_set, fd_set* write_set, int_frame_t* ctx) {
    fd_set* except_set = (fd_set*)ctx->r10;
    timespec* timeout = (timespec*)ctx->r8;
    sigset_t* sigmask = (sigset_t*)ctx->r9;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    DEBUG(proc->is_debug,"Trying to pselect num_fds %d, read_set 0x%p, write_set 0x%p, except_set 0x%p, timeout 0x%p from proc %d",num_fds,read_set,write_set,except_set,timeout,proc->id);

    SYSCALL_IS_SAFEZ(read_set,4096);
    SYSCALL_IS_SAFEZ(write_set,4096);
    SYSCALL_IS_SAFEZ(except_set,4096);
    SYSCALL_IS_SAFEZ(timeout,4096);

    pollfd fds[num_fds];
    memset(fds,0,sizeof(fds));

	int actual_count = 0;

	for(int fd = 0; fd < num_fds; ++fd) {
		short events = 0;
		if(read_set && FD_ISSET(fd, read_set)) {
			events |= POLLIN;
		}

		if(write_set && FD_ISSET(fd, write_set)) {
			events |= POLLOUT;
		}

		if(except_set && FD_ISSET(fd, except_set)) {
			events |= POLLIN;
		}

		if(events) {
			fds[actual_count].fd = fd;
			fds[actual_count].events = events;
			fds[actual_count].revents = 0;
			actual_count++;
		}
	}

	long long num;

    if(timeout) {
        num = sys_poll(fds, actual_count, (timeout->tv_sec * 1000) + (timeout->tv_nsec / (1000 * 1000)), ctx);
    } else {
        num = sys_poll(fds, actual_count, -1, ctx);
    }

    DEBUG(proc->is_debug,"pselect6 to poll status %lli",num);

    if(num < 0)
        return num;

	#define READ_SET_POLLSTUFF (POLLIN | POLLHUP | POLLERR)
	#define WRITE_SET_POLLSTUFF (POLLOUT | POLLERR)
	#define EXCEPT_SET_POLLSTUFF (POLLPRI)

	int return_count = 0;
	for(int fd = 0; fd < actual_count; ++fd) {
		int events = fds[fd].events;
		if((events & POLLIN) && (fds[fd].revents & READ_SET_POLLSTUFF) == 0) {
			FD_CLR(fds[fd].fd, read_set);
			events &= ~POLLIN;
		}

		if((events & POLLOUT) && (fds[fd].revents & WRITE_SET_POLLSTUFF) == 0) {
			FD_CLR(fds[fd].fd, write_set);
			events &= ~POLLOUT;
		}

		if(events)
			return_count++;
	}
	return return_count;
}

long long sys_readlinkat(int dirfd, const char* path, void* buffer, int_frame_t* ctx) {
    std::size_t max_size = ctx->r10;

    SYSCALL_IS_SAFEZ((void*)path,4096);
    SYSCALL_IS_SAFEZ((void*)buffer,max_size);
    
    if(!path || !buffer || !max_size)
        return -EINVAL;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(!vfs::fdmanager::search(proc,dirfd) && dirfd != AT_FDCWD) { DEBUG(1,"AAA %d\n",dirfd);
        return -EBADF; }


    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    char readlink_buf[2048];
    memset(readlink_buf,0,2048);
    vfs::vfs::lock();
    int ret = vfs::vfs::extern_readlink(result,readlink_buf,2048);
    vfs::vfs::unlock();

    if(ret != 0) {
        return -ret;
    }

    copy_in_userspace(proc,buffer,readlink_buf, strlen(readlink_buf) > max_size ? max_size : strlen(readlink_buf));

    return (int64_t)(strlen(readlink_buf) > max_size ? max_size : strlen(readlink_buf));
}

long long sys_readlink(char* path, void* buffer, int max_size) {

    int dirfd = AT_FDCWD;

    SYSCALL_IS_SAFEZ((void*)path,4096);
    SYSCALL_IS_SAFEZ((void*)buffer,max_size);

    if(!path || !buffer || !max_size)
        return -EINVAL;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    char readlink_buf[2048];
    memset(readlink_buf,0,2048);
    vfs::vfs::lock();
    int ret = vfs::vfs::extern_readlink(result,readlink_buf,2048);
    vfs::vfs::unlock();

    if(ret != 0) {
        return -ret;
    }

    copy_in_userspace(proc,buffer,readlink_buf, strlen(readlink_buf) > max_size ? max_size : strlen(readlink_buf));

    return (int64_t)(strlen(readlink_buf) > max_size ? max_size : strlen(readlink_buf));
}

long long sys_link(char* old_path, char* new_path) {

    SYSCALL_IS_SAFEZ((void*)old_path,2048);
    SYSCALL_IS_SAFEZ((void*)new_path,2048);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(!old_path || !new_path)
        return -EINVAL;

    char first_path[2048];
    memset(first_path,0,2048);
    if(0)
        memcpy(first_path,vfs::fdmanager::search(proc,0)->path,strlen(vfs::fdmanager::search(proc,0)->path));
    else if(1)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)old_path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    char first_path2[2048];
    memset(first_path2,0,2048);
    if(0)
        memcpy(first_path2,vfs::fdmanager::search(proc,0)->path,strlen(vfs::fdmanager::search(proc,0)->path));
    else if(1)
        memcpy(first_path2,proc->cwd,strlen(proc->cwd));

    char kpath2[2048];
    memset(kpath2,0,2048);
    copy_in_userspace_string(proc,kpath2,(void*)new_path,2048);

    char result2[2048];
    memset(result2,0,2048);
    vfs::resolve_path(kpath2,first_path2,result2,1,0);

    int ret = vfs::vfs::create(result2,VFS_TYPE_SYMLINK);

    if(ret != 0)
        return ret;

    userspace_fd_t fd;
    fd.is_cached_path = 0;
    fd.offset = 0;
    memset(fd.path,0,2048);
    memcpy(fd.path,result2,strlen(result2));

    ret = vfs::vfs::write(&fd,result,2047);

    return 0;

}

long long sys_linkat(int oldfd, char* old_path, int newfd, int_frame_t* ctx) {

    char* new_path = (char*)ctx->r10;

    SYSCALL_IS_SAFEZ((void*)old_path,2048);
    SYSCALL_IS_SAFEZ((void*)new_path,2048);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(!old_path || !new_path)
        return -EINVAL;

    char first_path[2048];
    memset(first_path,0,2048);
    if(oldfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,oldfd)->path,strlen(vfs::fdmanager::search(proc,oldfd)->path));
    else if(oldfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)old_path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    char first_path2[2048];
    memset(first_path2,0,2048);
    if(oldfd >= 0)
        memcpy(first_path2,vfs::fdmanager::search(proc,newfd)->path,strlen(vfs::fdmanager::search(proc,newfd)->path));
    else if(oldfd == AT_FDCWD)
        memcpy(first_path2,proc->cwd,strlen(proc->cwd));

    char kpath2[2048];
    memset(kpath2,0,2048);
    copy_in_userspace_string(proc,kpath2,(void*)new_path,2048);

    char result2[2048];
    memset(result2,0,2048);
    vfs::resolve_path(kpath2,first_path2,result2,1,0);

    int ret = vfs::vfs::create(result2,VFS_TYPE_SYMLINK);

    if(ret != 0)
        return ret;

    userspace_fd_t fd;
    fd.is_cached_path = 0;
    fd.offset = 0;
    memset(fd.path,0,2048);
    memcpy(fd.path,result2,strlen(result2));

    ret = vfs::vfs::write(&fd,result,2047);

    return 0;

}

long long sys_symlink(char* old, char* path) {
    return sys_symlinkat(old,AT_FDCWD,path);
}

long long sys_symlinkat(char* old, int dirfd, char* path) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    SYSCALL_IS_SAFEZ(old,4096);
    SYSCALL_IS_SAFEZ(path,4096);

    if(!old || !path)
        return -EINVAL;

    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    if(old[0] == '\0')
        return -EINVAL;

    int ret = vfs::vfs::create(result,VFS_TYPE_SYMLINK);

    if(ret != 0)
        return ret;

    userspace_fd_t fd;
    fd.is_cached_path = 0;
    fd.offset = 0;
    memset(fd.path,0,2048);
    memcpy(fd.path,result,strlen(result));

    ret = vfs::vfs::write(&fd,old,strlen(old) + 1);

    DEBUG(1,"%s (%s) to %s\n",result,path,old);

    return 0;

}

long long sys_mkdir(char* path, int mode) {
    return sys_mkdirat(AT_FDCWD,path,mode);
}

long long sys_mkdirat(int dirfd, char* path, int mode) {
    SYSCALL_IS_SAFEZ((void*)path,4096);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    int ret = vfs::vfs::create(result,VFS_TYPE_DIRECTORY);

    //DEBUG(1,"mkdir %s %d\n",result,ret);

    userspace_fd_t fd;
    fd.is_cached_path = 0;
    memcpy(fd.path,result,2048);

    vfs::vfs::var(&fd,mode,TMPFS_VAR_CHMOD | (1 << 7));
    return ret;
}

long long sys_chmod(char* path, int mode) {
    SYSCALL_IS_SAFEZ((void*)path,0);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char result[2048];
    memset(result,0,2048);

    if(!path)
        return -EINVAL;

    copy_in_userspace_string(proc,result,path,2048);

    userspace_fd_t fd;
    fd.is_cached_path = 0;
    memset(&fd,0,sizeof(fd));
    memcpy(fd.path,result,2048);

    DEBUG(proc->is_debug,"chmod %s %d\n",path,mode);

    uint64_t value;
    int ret = vfs::vfs::var(&fd,(uint64_t)&value,TMPFS_VAR_CHMOD);

    if(ret != 0)
        return -ret;

    ret = vfs::vfs::var(&fd,value | mode, TMPFS_VAR_CHMOD | (1 << 7));

    return -ret;
}

long long sys_fchmod(int fd, int mode) {

    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return -EBADF;

    uint64_t value;
    int ret = vfs::vfs::var(fd_s,(uint64_t)&value,TMPFS_VAR_CHMOD);

    if(ret != 0)
        return -ret;

    ret = vfs::vfs::var(fd_s,value | mode, TMPFS_VAR_CHMOD | (1 << 7));

    return -ret;
}

long long sys_fchmodat(int dirfd, const char* path, int mode, int_frame_t* ctx) {
    SYSCALL_IS_SAFEZ((void*)path,4096);
    if(!path)
        return -EFAULT;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    memset(first_path,0,2048);
    if(dirfd >= 0)
        memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD && proc->cwd)
        memcpy(first_path,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)path,2048);

    char result[2048];
    memset(result,0,2048);
    vfs::resolve_path(kpath,first_path,result,1,0);

    userspace_fd_t fd;
    fd.is_cached_path = 0;
    memset(&fd,0,sizeof(fd));
    memcpy(fd.path,result,2048);

    uint64_t value;
    int ret = vfs::vfs::var(&fd,(uint64_t)&value,TMPFS_VAR_CHMOD);

    if(ret != 0)
        return -ret;

    DEBUG(proc->is_debug,"chmodz %s %d\n",path,mode);

    ret = vfs::vfs::var(&fd,value | mode, TMPFS_VAR_CHMOD | (1 << 7));

    return -ret;

}

long long sys_fchmodat2(int dirfd, const char* path, int mode, int_frame_t* ctx) {

    int flags = ctx->r10 & 0xffffffff;

    SYSCALL_IS_SAFEZ((void*)path,4096);
    if(!path)
        return -EFAULT;

    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_sz = vfs::fdmanager::search(proc,dirfd);
    userspace_fd_t fd_tt;
    userspace_fd_t* fd_s = &fd_tt;

    if(!fd_sz && dirfd != -100)
        return -EBADF;

    if(flags & AT_EMPTY_PATH && !path) {
        if(!fd_sz)
            return -EBADF;
        fd_s = fd_sz;
    } else {

        if(!path)
            return -EINVAL;

        char first_path[2048];
        memset(first_path,0,2048);
        if(dirfd >= 0)
            memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
        else if(dirfd == AT_FDCWD && proc->cwd)
            memcpy(first_path,proc->cwd,strlen(proc->cwd));

        char kpath[2048];
        memset(kpath,0,2048);
        copy_in_userspace_string(proc,kpath,(void*)path,2048);

        char result[2048];
        memset(result,0,2048);
        vfs::resolve_path(kpath,first_path,result,1,0);
        memset(&fd_tt,0,sizeof(userspace_fd_t));
        memcpy(fd_tt.path,result,strlen(result) + 1);
        fd_tt.state = USERSPACE_FD_STATE_FILE;
    }


    uint64_t value;
    int ret = vfs::vfs::var(fd_s,(uint64_t)&value,TMPFS_VAR_CHMOD);

    if(ret != 0)
        return -ret;

    DEBUG(proc->is_debug,"chmodz %s %d\n",path,mode);

    ret = vfs::vfs::var(fd_s,value | mode, TMPFS_VAR_CHMOD | (1 << 7));

    return -ret;

}

syscall_ret_t sys_ttyname(int fd, char *buf, size_t size) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {0,EBADF,0};

    if(!buf)
        return {0,EINVAL,0};

    SYSCALL_IS_SAFEA(buf,2048);

    int ret = vfs::vfs::var(fd_s,0,DEVFS_VAR_ISATTY);

    fd_s->is_a_tty = ret == 0 ? 1 : 0;

    if(!fd_s->is_a_tty)
        return {0,ENOTTY,0};

    zero_in_userspace(proc,buf,size);
    if(strlen(fd_s->path) > size)
        return {0,ERANGE,0};

    copy_in_userspace(proc,buf,fd_s->path,strlen(fd_s->path));
    return {0,0,0};
}

long long sys_renameat(int olddir, char* old, int newdir, int_frame_t* ctx) {
    char* newp = (char*)ctx->r10;
    SYSCALL_IS_SAFEZ((void*)old,0);
    SYSCALL_IS_SAFEZ((void*)newp,0);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    int dirfd = olddir;

    char old_built[2048];
    memset(old_built,0,2048);
    if(dirfd >= 0)
        memcpy(old_built,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD && proc->cwd)
        memcpy(old_built,proc->cwd,strlen(proc->cwd));

    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)old,2048);

    char old_path_resolved[2048];
    memset(old_path_resolved,0,2048);
    vfs::resolve_path(kpath,old_built,old_path_resolved,1,0);

    dirfd = newdir;

    char new_built[2048];
    memset(new_built,0,2048);
    if(dirfd >= 0)
        memcpy(new_built,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
    else if(dirfd == AT_FDCWD && proc->cwd)
        memcpy(new_built,proc->cwd,strlen(proc->cwd));

    char kzpath[2048];
    memset(kzpath,0,2048);
    copy_in_userspace_string(proc,kzpath,(void*)newp,2048);

    char new_path_resolved[2048];
    memset(new_path_resolved,0,2048);
    vfs::resolve_path(kzpath,new_built,new_path_resolved,1,0);

    int status = vfs::vfs::rename(old_path_resolved,new_path_resolved);

    return status;
}

long long sys_rename(char* old, char* newp) {
    SYSCALL_IS_SAFEZ((void*)old,4096);
    SYSCALL_IS_SAFEZ((void*)newp,4096);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    char first_path[2048];
    memset(first_path,0,2048);
    memcpy(first_path,proc->cwd,strlen(proc->cwd));  
    char kpath[2048];
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)old,2048);
    char old_path[2048];
    memset(old_path,0,2048);
    vfs::resolve_path(kpath,first_path,old_path,1,0);
    if(old_path[0] == '\0') {
        old_path[0] = '/';
        old_path[1] = '\0';
    }

    memset(first_path,0,2048);
    memcpy(first_path,proc->cwd,strlen(proc->cwd));  
    memset(kpath,0,2048);
    copy_in_userspace_string(proc,kpath,(void*)newp,2048);
    char new_path[2048];
    memset(new_path,0,2048);
    vfs::resolve_path(kpath,first_path,new_path,1,0);
    if(new_path[0] == '\0') {
        new_path[0] = '/';
        new_path[1] = '\0';
    }

    int status = vfs::vfs::rename(old_path,new_path);

    return status;
}

typedef __SIZE_TYPE__    __mlibc_size;

struct iovec {
	void *iov_base;
	__mlibc_size iov_len;
};

typedef unsigned socklen_t;

struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int __pad0;
#endif
	int msg_iovlen;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	int __pad0;
#endif
	void *msg_control;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int __pad1;
#endif
	socklen_t msg_controllen;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	int __pad1;
#endif
	int msg_flags;
};

struct cmsghdr {
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int __pad;
#endif
	socklen_t cmsg_len;
#if __INTPTR_WIDTH__ == 64 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	int __pad;
#endif
	int cmsg_level;
	int cmsg_type;
};

#define SOL_SOCKET      1

#define SCM_RIGHTS 1
#define SCM_CREDENTIALS 2

#define __CMSG_ALIGN(s) (((s) + __alignof__(size_t) - 1) & \
		~(__alignof__(size_t) - 1))

#if defined(_DEFAULT_SOURCE)
#define CMSG_ALIGN(s) __CMSG_ALIGN(s)
#endif /* defined(_DEFAULT_SOURCE) */

/* Basic macros to return content and padding size of a control message. */
#define CMSG_LEN(s) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + (s))
#define CMSG_SPACE(s) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(s))

/* Provides access to the data of a control message. */
#define CMSG_DATA(c) ((unsigned char *)(c) + __CMSG_ALIGN(sizeof(struct cmsghdr)))

#define __MLIBC_CMSG_NEXT(c) ((char *)(c) + __CMSG_ALIGN((c)->cmsg_len))
#define __MLIBC_MHDR_LIMIT(m) ((char *)(m)->msg_control + (m)->msg_controllen)

/* For parsing control messages only. */
/* Returns a pointer to the first header or nullptr if there is none. */
#define CMSG_FIRSTHDR(m) ((size_t)(m)->msg_controllen <= sizeof(struct cmsghdr) \
	? (struct cmsghdr *)0 : (struct cmsghdr *) (m)->msg_control)

/* For parsing control messages only. */
/* Returns a pointer to the next header or nullptr if there is none. */
#define CMSG_NXTHDR(m, c) \
	((c)->cmsg_len < sizeof(struct cmsghdr) || \
		(std::int64_t)(sizeof(struct cmsghdr) + __CMSG_ALIGN((c)->cmsg_len)) \
			>= __MLIBC_MHDR_LIMIT(m) - (char *)(c) \
	? (struct cmsghdr *)0 : (struct cmsghdr *)__MLIBC_CMSG_NEXT(c))

syscall_ret_t sys_msg_send(int fd, struct msghdr* hdr, int flags) {
    struct msghdr* msg = hdr;
    SYSCALL_IS_SAFEA(hdr,sizeof(struct msghdr));
    
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {1,EBADF,0};

    vfs::pipe* target_pipe = fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? fd_s->read_socket_pipe : fd_s->write_socket_pipe;
    if(!target_pipe)
        return {1,EBADF,0};

    std::uint64_t total_size = 0;
    for (int i = 0; i < hdr->msg_iovlen; i++) {
        SYSCALL_IS_SAFEA(hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len);
        total_size += hdr->msg_iov[i].iov_len;
    }

    if(total_size > target_pipe->total_size)
        return {1,EMSGSIZE,0};

    target_pipe->lock.lock();
    std::uint64_t space_left = target_pipe->total_size - target_pipe->size;
    target_pipe->lock.unlock();
    while(space_left < total_size) {
        yield();
        target_pipe->lock.lock();
        space_left = target_pipe->total_size - target_pipe->size;
        target_pipe->lock.unlock();
    }

    target_pipe->lock.lock();

    struct cmsghdr *cmsg = 0;

    for (cmsg = CMSG_FIRSTHDR(msg);
        cmsg != NULL;
        cmsg = CMSG_NXTHDR(msg, cmsg)) {
             Log::SerialDisplay(LEVEL_MESSAGE_FAIL,"hello %d\n",cmsg->cmsg_type);
        
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            DEBUG(1,"scm\n");
            int new_fd = 0;
            memcpy(&new_fd, CMSG_DATA(cmsg), sizeof(int));

            userspace_fd_t* fd_s1 = vfs::fdmanager::search(proc,new_fd);

            if(!fd_s1) {
                break;
            }

            proc->pass_fd->push(fd_s1);
            target_pipe->fd_pass = (void*)proc->pass_fd; // transfer
        
        } else if(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
            target_pipe->ucred_pass->push((struct ucred*)CMSG_DATA(cmsg));
        }
    }

    std::int64_t total_written = 0;

    for (int i = 0; i < hdr->msg_iovlen; i++) {
        std::int64_t sent_bytes = target_pipe->nolock_write((const char*)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len,0);
        if(sent_bytes < 0) {
            target_pipe->lock.unlock();
            return {1,+sent_bytes,0};
        }
        total_written += sent_bytes;
        if (sent_bytes < hdr->msg_iov[i].iov_len) {
            break; 
        }
    }

    DEBUG(proc->is_debug,"msg_send fd %d total_written %lli flags %d from proc %d\n",fd,total_written,flags,proc->id);

    target_pipe->lock.unlock();
    return {1,0,total_written};
}

long long sys_msg_recv(int fd, struct msghdr *hdr, int flags) {
    struct msghdr* msg = hdr;
    SYSCALL_IS_SAFEZ(hdr,4096);
    
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return -EBADF;

    vfs::pipe* target_pipe = fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? fd_s->write_socket_pipe : fd_s->read_socket_pipe;
    if(!target_pipe)
        return -EBADF;

    std::uint64_t total_size = 0;
    for (int i = 0; i < hdr->msg_iovlen; i++) {
        SYSCALL_IS_SAFEZ(hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len);
        total_size += hdr->msg_iov[i].iov_len;
    }

    std::int64_t total_read = 0;

    for (int i = 0; i < hdr->msg_iovlen; i++) {
        std::int64_t recv_bytes = 0;
        recv_bytes = target_pipe->read(&fd_s->read_counter,(char*)hdr->msg_iov[i].iov_base,hdr->msg_iov[i].iov_len,((fd_s->flags & O_NONBLOCK) ? 1 : 0));
        if(recv_bytes == -EAGAIN) {
            return -EAGAIN;
        } else if(recv_bytes == -EAGAIN)
            break; // just dont continue 
        total_read += recv_bytes;
    }

    socklen_t new_msglen = 0;
    socklen_t src_msglen = hdr->msg_controllen;

    struct cmsghdr* cmsg = 0;

    target_pipe->lock.lock();
    for (cmsg = CMSG_FIRSTHDR(msg);
        cmsg != 0;
        cmsg = CMSG_NXTHDR(msg, cmsg)) {
        if (1) {
            int *fd_ptr = (int *) CMSG_DATA(cmsg);
            userspace_fd_t fd_s1;
            memset(&fd_s1,0,sizeof(userspace_fd_t));
            vfs::passingfd_manager* pasfd = (vfs::passingfd_manager*)target_pipe->fd_pass;
            if(pasfd != 0) {
                if(pasfd->pop(&fd_s1) == 0) {
                    cmsg->cmsg_level = SOL_SOCKET;
                    cmsg->cmsg_type = SCM_RIGHTS;
                    cmsg->cmsg_len = 24;
                    new_msglen += 24;
                    int new_fd = vfs::fdmanager::create(proc);
                    userspace_fd_t* nfd_s = vfs::fdmanager::search(proc,new_fd);

                    nfd_s->cycle = fd_s1.cycle;
                    nfd_s->offset = fd_s1.offset;
                    nfd_s->state = fd_s1.state;
                    nfd_s->other_state = fd_s1.other_state;
                    nfd_s->pipe = fd_s1.pipe;
                    nfd_s->pipe_side = fd_s1.pipe_side;
                    nfd_s->queue = fd_s1.queue;
                    nfd_s->is_a_tty = fd_s1.is_a_tty;
                    nfd_s->read_counter = fd_s1.read_counter;
                    nfd_s->write_counter = fd_s1.write_counter;
                    nfd_s->can_be_closed = 0;
                    nfd_s->write_socket_pipe = fd_s1.write_socket_pipe;
                    nfd_s->read_socket_pipe = fd_s1.read_socket_pipe;
                    nfd_s->eventfd = fd_s1.eventfd;
                    nfd_s->flags = fd_s1.flags;
                   
                    memcpy(nfd_s->path,fd_s1.path,sizeof(fd_s1.path));

                    memcpy(CMSG_DATA(cmsg), &new_fd, sizeof(int));
                    if(nfd_s->state == USERSPACE_FD_STATE_PIPE)
                        nfd_s->pipe->create(nfd_s->pipe_side);
                    else if(nfd_s->state == USERSPACE_FD_STATE_EVENTFD)
                        nfd_s->eventfd->create();
                } 
            } else {
                // fd pass is more important than ucred
                if(target_pipe->ucred_pass->pop((struct ucred*)CMSG_DATA(cmsg)) == 0) {
                    DEBUG(1,"ucred pass");
                    cmsg->cmsg_level = SOL_SOCKET;
                    cmsg->cmsg_type = SCM_CREDENTIALS;
                    cmsg->cmsg_len = 28;
                    new_msglen += 28;
                }
            }

        }
    }

    if(new_msglen != 0) {
        target_pipe->fd_pass = 0; // clear
        target_pipe->lock.unlock();
    } else
        target_pipe->lock.unlock();

    target_pipe->lock.unlock();
    hdr->msg_controllen = new_msglen;

    DEBUG(proc->is_debug,"msg_recv fd %d total_read %lli flags %d from proc %d, controllen %d srclen %d\n",fd,total_read,flags,proc->id,hdr->msg_controllen,src_msglen);
    return total_read;
}

syscall_ret_t sys_eventfd_create(unsigned int initval, int flags) {
    arch::x86_64::process_t* proc = CURRENT_PROC;

    int new_fd = vfs::fdmanager::create(proc);

    userspace_fd_t* new_fd_s = vfs::fdmanager::search(proc,new_fd);
    memset(new_fd_s->path,0,2048);

    new_fd_s->offset = 0;
    new_fd_s->queue = 0;
    new_fd_s->pipe = 0;
    new_fd_s->pipe_side = 0;
    new_fd_s->cycle = 0;
    new_fd_s->is_a_tty = 0;

    new_fd_s->state = USERSPACE_FD_STATE_EVENTFD;
    new_fd_s->eventfd = new vfs::eventfd(initval,flags);

    new_fd_s->eventfd->create();

    DEBUG(proc->is_debug,"Creating eventfd %d from proc %d, proc->fd 0x%p",new_fd,proc->id,proc->fd);

    return {1,0,new_fd};
}

std::uint64_t nextz = 2;

std::uint64_t __rand0() {
    uint64_t t = __rdtsc();
    return t * (++nextz);
}

syscall_ret_t sys_getentropy(char* buffer, std::uint64_t len) {
    SYSCALL_IS_SAFEA(buffer,256);
    if(!buffer)
        return {0,EFAULT,0};

    if(len > 256)
        return {0,EIO,0};

    for(std::uint64_t i = 0; i < len; i++) {
        ((char*)buffer)[i] = __rand0() & 0xFF;
    }

    return {0,0,0};
}

long long sys_pwrite(int fd, void* buf, std::uint64_t n, int_frame_t* ctx) {
    std::uint64_t off = ctx->r10;
    SYSCALL_IS_SAFEZ(buf,n);
    
    arch::x86_64::process_t* proc = CURRENT_PROC;

    std::uint64_t count = n;

    userspace_fd_t* fd_sz = vfs::fdmanager::search(proc,fd);
    if(!fd_sz)
        return -EBADF;

    userspace_fd_t fd_ss = *fd_sz;
    fd_ss.offset = off;

    userspace_fd_t* fd_s = &fd_ss;

    if(fd_s->can_be_closed)
        fd_s->can_be_closed = 0;

    char* temp_buffer = (char*)buf;   

    const char* _0 = "Content view is disabled in files";
    DEBUG(proc->is_debug && fd != 1 && fd != 2,"Writing %s with content %s fd %d state %d from proc %d count %d writ sock 0x%p",fd_s->state == USERSPACE_FD_STATE_FILE ? fd_s->path : "Not file",buf,fd,fd_s->state,proc->id,count,fd_s->write_socket_pipe);

    if(fd == 1) {
        //Log::SerialDisplay(LEVEL_MESSAGE_INFO,"%s\n",buf);
    }

    std::int64_t bytes_written;
    if(fd_s->state == USERSPACE_FD_STATE_FILE)
        bytes_written = vfs::vfs::write(fd_s,temp_buffer,count);
    else if(fd_s->state == USERSPACE_FD_STATE_PIPE) {
        if(fd_s->pipe_side != PIPE_SIDE_WRITE)
            return -EBADF;
        bytes_written = fd_s->pipe->write(temp_buffer,count,proc->id);
    } else if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {

        if(!fd_s->write_socket_pipe || !fd_s->read_socket_pipe)
            return -EFAULT;
        
        if(fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
            bytes_written = fd_s->read_socket_pipe->write(temp_buffer,count,proc->id);
        } else {
            bytes_written = fd_s->write_socket_pipe->write(temp_buffer,count,proc->id);
        }

        if(bytes_written == 0)
            return 0;

    } else if(fd_s->state == USERSPACE_FD_STATE_EVENTFD) {
        if(count != 8)
            return -EINVAL;
        std::uint64_t* _8sizebuf = (std::uint64_t*)buf;
        bytes_written = fd_s->eventfd->write(*_8sizebuf);
    } else
        return -EBADF;
        

    return bytes_written;

}

typedef struct __mlibc_fsid {
	int __val[2];
} fsid_t;

typedef std::uint64_t fsblkcnt_t;
typedef std::uint64_t fsfilcnt_t;

/* WARNING: keep `statfs` and `statfs64` in sync or bad things will happen! */
struct statfs {
	unsigned long f_type;
	unsigned long f_bsize;
	fsblkcnt_t f_blocks;
	fsblkcnt_t f_bfree;
	fsblkcnt_t f_bavail;
	fsfilcnt_t f_files;
	fsfilcnt_t f_ffree;
	fsid_t f_fsid;
	unsigned long f_namelen;
	unsigned long f_frsize;
	unsigned long f_flags;
	unsigned long __f_spare[4];
};

long long sys_statfs(char* path, struct statfs* buf) {
    SYSCALL_IS_SAFEZ(path,4096);
    SYSCALL_IS_SAFEZ(buf,4096);
    arch::x86_64::process_t* proc = CURRENT_PROC;

    if(!path || !buf)
        return -EINVAL;

    buf->f_type = 0xEF53; // just default to ext2
    buf->f_bsize = 4096;
    buf->f_blocks = 0;
    buf->f_bfree = 0;
    buf->f_bavail = 0;

    extern std::uint64_t __tmpfs_ptr_id;

    buf->f_files = __tmpfs_ptr_id;
    buf->f_ffree = 0xffffffffffffffff - __tmpfs_ptr_id;
    buf->f_fsid = {0,0};
    buf->f_namelen = 2048;
    buf->f_frsize = 0;
    buf->f_flags = 0;

    return 0;
}

long long sys_fstatfs(int fd, struct statfs *buf) {
    SYSCALL_IS_SAFEZ(buf,4096);
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return -EBADF;

    buf->f_type = 0xEF53; // just default to ext2
    buf->f_bsize = 4096;
    buf->f_blocks = 0;
    buf->f_bfree = 0;
    buf->f_bavail = 0;

    extern std::uint64_t __tmpfs_ptr_id;

    buf->f_files = __tmpfs_ptr_id;
    buf->f_ffree = 0xffffffffffffffff - __tmpfs_ptr_id;
    buf->f_fsid = {0,0};
    buf->f_namelen = 2048;
    buf->f_frsize = 0;
    buf->f_flags = 0;

    return 0;

}

long long sys_statx(int dirfd, const char *path, int flag, int_frame_t* ctx) {
    SYSCALL_IS_SAFEZ((void*)path,4096);

    arch::x86_64::process_t* proc = CURRENT_PROC;

    std::uint32_t mask = ctx->r10 & 0xFFFFFFFF;
    statx_t* out = (statx_t*)ctx->r8;

    userspace_fd_t* fd_sz = vfs::fdmanager::search(proc,dirfd);
    userspace_fd_t fd_tt;
    userspace_fd_t* fd_s = &fd_tt;

    int flags = ctx->r10;

    if(!fd_sz && dirfd != -100)
        return -EBADF;

    if(flags & AT_EMPTY_PATH && !path) {
        if(!fd_sz)
            return -EBADF;
        fd_s = fd_sz;
    } else {

        if(!path)
            return -EINVAL;

        char first_path[2048];
        memset(first_path,0,2048);
        if(dirfd >= 0)
            memcpy(first_path,vfs::fdmanager::search(proc,dirfd)->path,strlen(vfs::fdmanager::search(proc,dirfd)->path));
        else if(dirfd == AT_FDCWD && proc->cwd)
            memcpy(first_path,proc->cwd,strlen(proc->cwd));

        char kpath[2048];
        memset(kpath,0,2048);
        copy_in_userspace_string(proc,kpath,(void*)path,2048);

        char result[2048];
        memset(result,0,2048);
        vfs::resolve_path(kpath,first_path,result,1,0);
        memset(&fd_tt,0,sizeof(userspace_fd_t));
        memcpy(fd_tt.path,result,strlen(result) + 1);
    }

    DEBUG(proc->is_debug,"trying to statx %s\n",path);
    return vfs::vfs::statx(fd_s,flag,mask,out);
}

long long sys_getdents64(int fd, char* buf, size_t count) {
    SYSCALL_IS_SAFEZ(buf,count);
    
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return -EBADF;

    if(!buf)
        return -EINVAL;

    DEBUG(proc->is_debug,"getdents64 fd %d buf 0x%p, count %lli",fd,buf,count);

    memset(buf,0,count);

    std::uint64_t off = 0;
    while(1) {
        vfs::dirent_t dirent = {0};
        int status = vfs::vfs::ls(fd_s,&dirent);
        if(status < 0)
            return -status;

        if(dirent.d_reclen == 0) {
            break;
        }

        std::uint64_t built_size = strlen(dirent.d_name) + sizeof(linux_dirent64) + 1;
        if(built_size > count - off)
            break;

        linux_dirent64* dirent64 = (linux_dirent64*)(buf + off);
        dirent64->d_ino = dirent.d_ino;
        dirent64->d_type = dirent.d_type;
        dirent64->d_off = 0;
        dirent64->d_reclen = strlen(dirent.d_name) + sizeof(linux_dirent64) + 1;
        memcpy(dirent64->d_name,dirent.d_name,strlen(dirent.d_name) + 1);
        off += dirent64->d_reclen;

    }
    return off;
}

long long sys_umask(int mask) {
    return 0;
}