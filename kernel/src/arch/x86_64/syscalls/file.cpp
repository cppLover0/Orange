
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <generic/vfs/vfs.hpp>

#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/scheduling.hpp>

#include <generic/mm/pmm.hpp>

#include <generic/vfs/fd.hpp>

#include <generic/vfs/devfs.hpp>

#include <etc/etc.hpp>
#include <etc/errno.hpp>

#include <etc/logging.hpp>

#include <cstdint>

syscall_ret_t sys_openat(int dirfd, const char* path, int flags) {
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

    if(flags & O_CREAT)
        vfs::vfs::touch(new_fd_s->path);

    std::int32_t status = vfs::vfs::open(new_fd_s);

    vfs::stat_t stat;
    std::int32_t stat_status = vfs::vfs::stat(new_fd_s,&stat);

    if(stat_status == 0) 
        if(flags & __O_DIRECTORY) 
            if(!(stat.st_mode & S_IFDIR))
                return {0,ENOTDIR,0};

    if(flags & O_TRUNC) 
        vfs::vfs::write(new_fd_s,(void*)"\0",1);

    if(flags & O_APPEND)
        new_fd_s->offset = stat.st_size;

    if(status != 0)
        new_fd_s->state = USERSPACE_FD_STATE_UNUSED;
    
    new_fd_s->offset = 0;

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
        SYSCALL_ENABLE_PREEMPT();
        bytes_read = fd_s->pipe->read(temp_buffer,count);
        SYSCALL_DISABLE_PREEMPT();
    } else if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {

        if(!fd_s->write_socket_pipe || !fd_s->read_socket_pipe)
            return {1,EFAULT,0};

        if(fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
            SYSCALL_ENABLE_PREEMPT();
            bytes_read = fd_s->write_socket_pipe->read(temp_buffer,count);
            SYSCALL_DISABLE_PREEMPT();
        } else {
            SYSCALL_ENABLE_PREEMPT();
            bytes_read = fd_s->read_socket_pipe->read(temp_buffer,count);
            SYSCALL_DISABLE_PREEMPT();
        }
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
        SYSCALL_ENABLE_PREEMPT();
        bytes_written = fd_s->pipe->write(temp_buffer,count);
        SYSCALL_DISABLE_PREEMPT();
    } else if(fd_s->state == USERSPACE_FD_STATE_SOCKET) {

        if(!fd_s->write_socket_pipe || !fd_s->read_socket_pipe)
            return {1,EFAULT,0};

        if(fd_s->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
            SYSCALL_ENABLE_PREEMPT();
            bytes_written = fd_s->read_socket_pipe->write(temp_buffer,count);
            SYSCALL_DISABLE_PREEMPT();
        } else {
            SYSCALL_ENABLE_PREEMPT();
            bytes_written = fd_s->write_socket_pipe->write(temp_buffer,count);
            SYSCALL_DISABLE_PREEMPT();
        }
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

    if(fd_s->state == USERSPACE_FD_STATE_PIPE)
        return {1,0,0};

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

    if(!fd_s->is_a_tty && fd_s->index > 2)
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
        vfs::stat_t stat;
        memset(&stat,0,sizeof(vfs::stat_t));
        if(fd_s->state == USERSPACE_FD_STATE_SOCKET)
            stat.st_mode |= S_IFSOCK;
        else if(fd_s->state == USERSPACE_FD_STATE_PIPE)
            stat.st_mode |= S_IFIFO;
        copy_in_userspace(proc,out,&stat,sizeof(vfs::stat_t));
        return {0,0,0};
    }
    return {0,0,0};
}

syscall_ret_t sys_pipe(int flags) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    int read_fd = vfs::fdmanager::create(proc);
    int write_fd = vfs::fdmanager::create(proc);
    userspace_fd_t* fd1 = vfs::fdmanager::search(proc,read_fd);
    userspace_fd_t* fd2 = vfs::fdmanager::search(proc,write_fd);

    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"sys_pipe flags 0x%p\n",flags);
    vfs::pipe* new_pipe = new vfs::pipe(flags);
    fd1->pipe_side = PIPE_SIDE_READ;
    fd2->pipe_side = PIPE_SIDE_WRITE;

    fd1->pipe = new_pipe;
    fd2->pipe = new_pipe;

    fd1->state = USERSPACE_FD_STATE_PIPE;
    fd2->state = USERSPACE_FD_STATE_PIPE;

    return {1,read_fd,write_fd};
}

syscall_ret_t sys_dup(int fd, int flags) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    
    if(!fd_s)
        return {0,EBADF,0};
    
    int new_fd = vfs::fdmanager::create(proc);
    userspace_fd_t* nfd_s = vfs::fdmanager::search(proc,new_fd);

    nfd_s->cycle = fd_s->cycle;
    nfd_s->offset = fd_s->offset;
    nfd_s->state = fd_s->state;
    nfd_s->other_state = fd_s->other_state;
    nfd_s->pipe = fd_s->pipe;
    nfd_s->pipe_side = fd_s->pipe_side;
    nfd_s->queue = fd_s->queue;
    nfd_s->is_a_tty = fd_s->is_a_tty;
    memcpy(nfd_s->path,fd_s->path,sizeof(fd_s->path));

    return {1,0,new_fd};
}

syscall_ret_t sys_dup2(int fd, int flags, int newfd) {
    arch::x86_64::process_t* proc = CURRENT_PROC;

    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"dup2 from %d to %d in proc %d with flags 0x%p\n",fd,newfd,proc->id,flags);

    if(!fd_s)
        return {0,EBADF,0};

    userspace_fd_t* nfd_s = vfs::fdmanager::search(proc,newfd);
    if(!nfd_s) {
        int t = vfs::fdmanager::create(proc);
        nfd_s = vfs::fdmanager::search(proc,t);
    } else {
        if(nfd_s->state == USERSPACE_FD_STATE_PIPE) 
            nfd_s->pipe->close(nfd_s->pipe_side);
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

    if(nfd_s->state == USERSPACE_FD_STATE_PIPE)
        nfd_s->pipe->create(nfd_s->pipe_side);

    memcpy(nfd_s->path,fd_s->path,sizeof(fd_s->path));

    return {0,0,0};
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

syscall_ret_t sys_ioctl(int fd, unsigned long request, void *arg) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);

    if(!fd_s)
        return {0,EBADF,0};

    int res = 0;
    std::int64_t ioctl_size = vfs::vfs::ioctl(fd_s,request,0,&res);
    
    if(ioctl_size < 0)
        return {0,0,0};

    SYSCALL_IS_SAFEA((void*)arg,ioctl_size);

    char ioctl_item[ioctl_size]; /* Actually i can just allocate this in stack */
    copy_in_userspace(proc,ioctl_item,arg,ioctl_size);
 
    std::int64_t ret = vfs::vfs::ioctl(fd_s,request,ioctl_item,&res);
    copy_in_userspace(proc,arg,ioctl_item,ioctl_size);

    return {0,(int)ret,0};
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

    if(fd_s->state == USERSPACE_FD_STATE_PIPE)
        return {0,ENOTTY,0};

    int ret = vfs::vfs::var(fd_s,0,DEVFS_VAR_ISATTY);

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

    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {0,EBADF,0};

    vfs::dirent_t dirent;
    copy_in_userspace(proc,&dirent,buffer,sizeof(vfs::dirent_t));

    int status = vfs::vfs::ls(fd_s,&dirent);
        
    copy_in_userspace(proc,buffer,&dirent,sizeof(vfs::dirent_t));
    return {1,status,dirent.d_reclen};
    
}

syscall_ret_t sys_fcntl(int fd, int request, std::uint64_t arg) {
    Log::SerialDisplay(LEVEL_MESSAGE_INFO,"fcntl %d\n",request);
    switch(request) {
        case F_DUPFD: {
            return sys_dup(fd,arg);
        }

        case F_GETFL: {
            arch::x86_64::process_t* proc = CURRENT_PROC;
            userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
            if(!fd_s)
                return {0,EBADF,0};

            return {1,0,(std::int64_t)(fd_s->flags | (fd_s->state == USERSPACE_FD_STATE_PIPE ? fd_s->pipe->flags : 0))};
        }

        case F_SETFL: {
            arch::x86_64::process_t* proc = CURRENT_PROC;
            userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
            if(!fd_s)
                return {0,EBADF,0};

            fd_s->flags &= ~(O_APPEND | O_ASYNC | O_NONBLOCK);
            fd_s->flags |= (arg & (O_APPEND | O_ASYNC | O_NONBLOCK));

            if(fd_s->state == USERSPACE_FD_STATE_PIPE) {
                fd_s->pipe->flags & ~(O_NONBLOCK);
                fd_s->pipe->flags |= (arg & O_NONBLOCK);
            }

            return {1,0,0};
        }

        default: {
            return {0,0,0};
        }
    }
    return {0,ENOSYS,0};
}

syscall_ret_t sys_fchdir(int fd) {
    arch::x86_64::process_t* proc = CURRENT_PROC;
    userspace_fd_t* fd_s = vfs::fdmanager::search(proc,fd);
    if(!fd_s)
        return {0,EBADF,0};

    vfs::stat_t stat;
    int ret = vfs::vfs::stat(fd_s,&stat);

    if(ret != 0)
        return {0,ret,0};

    if(!(stat.st_mode & S_IFDIR))
        return {0,ENOTDIR,0};

    memset(proc->cwd,0,4096);
    memcpy(proc->cwd,fd_s->path,2048);

    return {0,0,0};

}