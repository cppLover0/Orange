#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <generic/userspace/syscall_list.hpp>
#include <generic/userspace/safety.hpp>
#include <generic/time.hpp>

long long sys_access(const char* path, int mode) {
    (void)mode;
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096)) {
        return -EFAULT;
    }

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    process_path(current->chroot, current->cwd, buffer1, buffer);

    if(current->is_debug)
        klibc::debug_printf("trying to access %s\n", buffer);

    file_descriptor fd = {};
    int status = vfs::open(&fd, buffer, true, false);
    if(status != 0)
        return status;

    if(fd.vnode.close)
        fd.vnode.close(&fd);

    return 0;
}

long long sys_openat(int dfd, const char* path, int flags, int mode) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096)) {
        return -EFAULT;
    }

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current, dfd);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, buffer);

    if(current->is_debug) {
        klibc::debug_printf("trying to open %s (%s + %s + %s) with flags 0x%p dfd %d mode 0x%p\n",buffer,current->chroot ? current->chroot : "no chroot", at, path, flags, dfd, mode);
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)(current->fd);
    file_descriptor* new_fd = manager->createlowest(2);

    int status = vfs::open(new_fd, buffer, true, flags & O_DIRECTORY);

    if(flags & O_CREAT && !(flags & O_DIRECTORY) && status == -ENOENT) {
        int create_status = vfs::create(buffer, vfs_file_type::file, mode);
        if(create_status != 0)
            goto fail;
        status = vfs::open(new_fd, buffer, true, flags & O_DIRECTORY);
        if(status != 0)
            goto fail;
        assert(new_fd->vnode.chmod, "v");
        new_fd->vnode.chmod(new_fd, mode);
    } 

fail:
    if(status != 0) {
        manager->close(new_fd);
        return status;
    }

    stat file_stat = {};

    if(new_fd->vnode.stat) {
        int status2 = new_fd->vnode.stat(new_fd, &file_stat);
        assert(status2 == 0, "chieknefpksgf'shdgko'psdgo'hks'o %s (%d)", buffer, status2);
        if(flags & O_TRUNC) {
            if(new_fd->vnode.zero) {
                new_fd->vnode.zero(new_fd);
            }
        }
        if(flags & O_APPEND)
            new_fd->offset = file_stat.st_size;
    }

    assert((std::uint64_t)(klibc::strlen(buffer) + 1) < sizeof(new_fd->path), "bruh.");
    klibc::memcpy(new_fd->path, buffer, klibc::strlen(buffer) + 1);

    new_fd->flags = flags;

    return new_fd->index;
}

long long sys_open(const char* path, int flags, int mode) {
    return sys_openat(AT_FDCWD, path, flags, mode);
}

long long sys_newfstatat(int dfd, const char* path, stat* out, int flags) {
    thread* current = current_proc;

    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096) && path) {
        return -EFAULT;
    }

    if(!is_safe_to_rw(current, (std::uint64_t)out, 4096)) {
        return -EFAULT;
    }

    klibc::memset(out, 0, sizeof(stat));

    if(current->is_debug) {
        klibc::debug_printf("trying to stat %d %s 0x%p\n", dfd, path ? path : "empty path", flags);
    }

    bool is_empty = false;
    if(!path)
        is_empty = true;

    if(path) {
        if(path[0] == '\0')
            is_empty = true;
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor target_fd = {};
    if(is_empty) {
        file_descriptor* fd = manager->search(dfd);
        if(!fd)
            return -EBADF;
        target_fd = *fd;
    } else {
        char buffer1[4096] = {};
        char buffer[4096] = {};
        klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

        char* at = at_to_char(current, dfd);
        if(at == nullptr)
            return -EBADF;

        process_path(current->chroot, at, buffer1, buffer);

        target_fd.type = file_descriptor_type::file;
        int status = vfs::open(&target_fd, buffer, flags & AT_SYMLINK_NOFOLLOW ? false : true, false);
        if(status != 0)
            return status;

    }

    if(target_fd.type == file_descriptor_type::pipe) {
        out->st_mode |= S_IFIFO;
        return 0;
    }

    assert(target_fd.vnode.stat, "no lol");

    target_fd.vnode.stat(&target_fd, out);
    if(current->is_debug) klibc::debug_printf("mode %o %s", out->st_mode, path == nullptr ? "" : path);

    if(!is_empty) {
        if(target_fd.vnode.close)
            target_fd.vnode.close(&target_fd);
    }

    return 0;
}

// just stat and return if ok
long long sys_faccessat2(int dfd, const char* path, int mode, int flags) {
    (void)mode;
    stat out = {};
    long long ret = sys_newfstatat(dfd, path, &out, flags);
    if(ret != 0)
        return ret;
    return 0;
}

long long sys_fstat(int fd, stat* out) {
    return sys_newfstatat(fd, nullptr, out, 0);
}

long long sys_statfs(const char* path, statfs* out) {
    thread* current_thread = current_proc;
    if(!is_safe_to_rw(current_thread, (std::uint64_t)out, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(current_thread, (std::uint64_t)path, PAGE_SIZE))
        return -EFAULT;

    stat tmp_stat = {};
    file_descriptor target_fd = {};
    char buffer[4096] = {};
    char buffer1[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current_thread, AT_FDCWD);
    if(at == nullptr)
        return -EBADF;

    process_path(current_thread->chroot, at, buffer1, buffer);

    target_fd.type = file_descriptor_type::file;
    int status = vfs::open(&target_fd, buffer, true, false);
    if(status != 0)
        return status;

    target_fd.vnode.stat(&target_fd, &tmp_stat);

    if(target_fd.vnode.close) target_fd.vnode.close(&target_fd);

    out->f_type = 0xEF53;
    out->f_bsize = tmp_stat.st_blksize;
    out->f_blocks = 0;
    out->f_bfree = 0;
    out->f_bavail = 0;
    out->f_files = 0;
    out->f_ffree = 0;
    out->f_namelen = 4096;

    return 0;
}

long long sys_read(int fd, char* buffer, std::uint64_t count) {
    if(count == 0)
        return 0;

    thread* current = current_proc;

    if(buffer == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)buffer, count)) {
        return -EFAULT;
    }

    if(current->is_debug) {
        klibc::debug_printf("trying to read fd %d buffer 0x%p count %lli\n", fd, buffer, count);
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    if(file->type == file_descriptor_type::file) {
        return file->vnode.read(file, buffer, count);
    } else if(file->type == file_descriptor_type::pipe) {
        return file->fs_specific.pipe->read(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
    }

    assert(0, "unimplemented read fd %d, type %d", fd, file->type);

    return -EFAULT;
}

long long sys_write(int fd, char* buffer, std::uint64_t count) {
    if(count == 0)
        return 0;

    thread* current = current_proc;

    if(buffer == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)buffer, count)) {
        return -EFAULT;
    }

    if(current->is_debug) {
        klibc::debug_printf("trying to write fd %d buffer 0x%p count %lli\n", fd, buffer, count);
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    if(file->type == file_descriptor_type::file) {
        return file->vnode.write(file, buffer, count);
    } else if(file->type == file_descriptor_type::pipe) {
        return file->fs_specific.pipe->write(buffer, count);
    }

    assert(0, "unimplemented write fd %d, type %d", fd, file->type);

    return -EFAULT;
}

long long sys_pread64(int fd, char* buffer, std::uint64_t count, std::uint64_t pos) {
    if(count == 0)
        return 0;

    thread* current = current_proc;

    if(buffer == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)buffer, count)) {
        return -EFAULT;
    }

    if(current->is_debug) {
        klibc::debug_printf("trying to pread64 fd %d buffer 0x%p count %lli pos %lli\n", fd, buffer, count, pos);
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    std::uint64_t old_offset = file->offset;
    file->offset = pos;
    
    if(file->type == file_descriptor_type::file) {
        long long ret = file->vnode.read(file, buffer, count);
        file->offset = old_offset;
        return ret;
    } 

    return -ESPIPE;
}

long long sys_close(int fd) {
    thread* current = current_proc;

    if(current->is_debug) {
        klibc::debug_printf("trying to close fd %d\n", fd);
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);

    if(!file)
        return -EBADF;

    manager->close(file);
    return 0;
}

long long sys_ioctl(int fd, std::uint64_t req, std::uint64_t arg) {
    thread* current = current_proc;

    if(current->is_debug) {
        klibc::debug_printf("trying to ioctl fd %d cmd %d arg 0x%p\n", fd, req, arg);
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);

    if(!file)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -ENOTTY;

    if(!file->vnode.ioctl)
        return -ENOTTY;

    if(!is_safe_to_rw(current, arg, PAGE_SIZE))
        return -EFAULT;

    return file->vnode.ioctl(file, req, (void*)arg);
}

long long sys_readlinkat(int dfd, const char* path, char* buf, int bufsize) {
    thread* current = current_proc;

    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096) && path) {
        return -EFAULT;
    }

    if(!is_safe_to_rw(current, (std::uint64_t)buf, 4096)) {
        return -EFAULT;
    }

    if(path == nullptr || buf == nullptr)
        return -EINVAL;

    if(bufsize <= 0)
        return 0;

    if(current->is_debug) {
        klibc::debug_printf("trying to readlinkat %d %s 0x%p\n", dfd, path ? path : "empty path", 0);
    }

    file_descriptor target_fd = {};
    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current, dfd);
    if(at == nullptr)
            return -EBADF;

    process_path(current->chroot, at, buffer1, buffer);

    target_fd.type = file_descriptor_type::file;

    int status = vfs::open(&target_fd, buffer, false, false);
    if(status != 0)
        return status;

    if(target_fd.type != file_descriptor_type::file)
        return -EINVAL;

    std::int64_t ret = target_fd.vnode.read(&target_fd, buf, bufsize);
    if(target_fd.vnode.close)
        target_fd.vnode.close(&target_fd);

    return ret;
}

long long sys_readlink(const char* path, char* buf, int size) {
    return sys_readlinkat(AT_FDCWD, path, buf, size);
}

long long sys_dup(int fd) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;
    file_descriptor* src = manager->search(fd);
    if(src == nullptr)
        return -EBADF;

    file_descriptor* new_fd = manager->createlowest(-1);
    manager->do_dup(src, new_fd);
    return new_fd->index;
}

long long sys_dup2(int old, int new_fd) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;
    file_descriptor* src = manager->search(old);
    if(src == nullptr)
        return -EBADF;

    file_descriptor* new_fd_s = manager->try_dup2(src, new_fd);
    return new_fd_s->index;
}

long long sys_fcntl(int fd, int request, std::uint64_t arg) {

    thread* proc = current_proc;
    auto manager = (vfs::fdmanager*)proc->fd;

    if(!is_safe_to_rw(proc, arg, PAGE_SIZE))
        return -EFAULT;

    if(proc->is_debug)
        klibc::debug_printf("fcntl fd %d req %d arg 0x%p from proc %d",fd,request,arg);
    int is_cloexec = 0;
    switch(request) {
        case F_DUPFD_CLOEXEC:
            is_cloexec = 1;
        case F_DUPFD: {
            file_descriptor* fd_s = manager->search(fd);
            if(fd_s == nullptr)
                return -EBADF;
            file_descriptor* new_fd = manager->createlowest(arg);
            manager->do_dup(fd_s, new_fd);
            if(proc->is_debug)
                klibc::debug_printf("return fd %d",new_fd->index);
            new_fd->other.is_cloexec = is_cloexec;
            return new_fd->index; 
        }

        case F_GETFD: {
            file_descriptor* fd_s = manager->search(fd);
            if(!fd_s)
                return -EBADF;
            return fd_s->other.is_cloexec;
        }

        case F_SETFD: {
            file_descriptor* fd_s = manager->search(fd);
            if(!fd_s)
                return -EBADF;

            fd_s->other.is_cloexec = arg & 1;
            return 0;
        }

        case F_GETFL: {
            file_descriptor* fd_s = manager->search(fd);
            if(!fd_s)
                return -EBADF;

            return (fd_s->flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
        }

        case F_SETFL: {
            file_descriptor* fd_s = manager->search(fd);

            if(!fd_s)
                return -EBADF;

            fd_s->flags &= ~(O_APPEND | O_ASYNC | O_NONBLOCK | O_RDONLY | O_RDWR | O_WRONLY);
            fd_s->flags |= (arg & (O_APPEND | O_ASYNC | O_NONBLOCK | O_RDONLY | O_RDWR | O_WRONLY));

            return 0;
        }

        default: {
            assert(0,"unsupported fcntl");
        }
    }
    return -EINVAL;
}

long long sys_pipe2(int* fds, int flags) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;
    if(!is_safe_to_rw(current, (std::uint64_t)fds, PAGE_SIZE))
        return -EFAULT;

    if(fds == nullptr)
        return -EINVAL;

    file_descriptor* fd0 = manager->createlowest(2);
    file_descriptor* fd1 = manager->createlowest(2);
    fd0->type = file_descriptor_type::pipe;
    fd1->type = file_descriptor_type::pipe;
    fd0->fs_specific.pipe = new vfs::pipe(flags);
    fd1->fs_specific.pipe = fd0->fs_specific.pipe;
    fd0->other.pipe_side = PIPE_SIDE_READ;
    fd1->other.pipe_side = PIPE_SIDE_WRITE;
    fds[0] = fd0->index;
    fds[1] = fd1->index;
    fd0->other.is_cloexec = (flags & __O_CLOEXEC) ? true : false; 
    fd1->other.is_cloexec = (flags & __O_CLOEXEC) ? true : false; 

    return 0;
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
    klibc::memcpy(out,result,klibc::strlen(result) + 1);
}

long long poll_impl(pollfd* fds, std::uint32_t nfds, int timeout) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;
    
    if(fds == nullptr)
        return -EINVAL;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
    file_descriptor* cached[nfds];
    klibc::memset(cached, 0, nfds * sizeof(file_descriptor*));
#pragma clang diagnostic pop

    for(std::uint32_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        file_descriptor* fd = manager->search(fds[i].fd);
        if(fd == nullptr)
            return -EBADF;

        cached[i] = fd;

        if(current->is_debug) {
            char type[64] = {};
            poll_to_str(fds[i].events, type);
            klibc::debug_printf("trying to poll fd %d, type %s (%d)\n", fds[i].fd, type ,fds[i].events);
        }
    }

    auto poll_body = [fds, nfds](file_descriptor** cached) -> int {
        int count = 0;
        for(std::uint32_t i = 0;i < nfds;i++) {
            file_descriptor* fd = cached[i];
            bool is_event = false;
            if(fds[i].events & POLLIN) {
                bool ret = false;
                if(fd->type == file_descriptor_type::file) {
                    if(fd->vnode.poll) {
                        ret = fd->vnode.poll(fd, vfs_poll_type::pollin);
                    } 
                } else if(fd->type == file_descriptor_type::pipe) {
                    if(fd->fs_specific.pipe->size.load() != 0) 
                        ret = true;
                }

                if(ret == true) {
                    fds[i].revents |= POLLIN;
                    is_event = true;
                }
            }
            
            if(fds[i].events & POLLOUT) {
                bool ret = false;
                if(fd->type == file_descriptor_type::file) {
                    if(fd->vnode.poll) {
                        ret = fd->vnode.poll(fd, vfs_poll_type::pollout);
                    } 
                } else if(fd->type == file_descriptor_type::pipe) {
                    if((std::uint64_t)fd->fs_specific.pipe->size.load() != fd->fs_specific.pipe->total_size) 
                        ret = true;
                }

                if(ret == true) {
                    fds[i].revents |= POLLOUT;
                    is_event = true;
                }
            }

            if(is_event)
                count++;
        }
        return count;
    };

    if(timeout == -1) {
        while(true) {
            int ret = poll_body(cached);
            if(ret != 0)
                return ret;
            process::yield();
        }
    } else {
        std::uint64_t current_timestamp = time::timer->current_nano() / 1000;
        std::uint64_t end_timestamp = (current_timestamp + (timeout * 1000));
        while(time::timer->current_nano() / 1000 < end_timestamp) {
            int ret = poll_body(cached);
            if(ret != 0)
                return ret;
            
            if(time::timer->current_nano() / 1000 < end_timestamp) {
                process::yield();
            }
        }
        return 0;
    }

    assert(0,"n");
    return -EFAULT;
}

long long sys_poll(pollfd* fds, std::uint32_t nfds, int timeout) {
    thread* current = current_proc;

    if(!is_safe_to_rw(current, (std::uint64_t)fds, (nfds * sizeof(pollfd)) + PAGE_SIZE))
        return -EFAULT;

    return poll_impl(fds, nfds, timeout);
}

long long sys_pselect6(int num_fds, fd_set* read_set, fd_set* write_set, fd_set* except_set, timespec* timeout, sigset_t* sigmask) {
    (void)sigmask;
    thread* proc = current_proc;

    if(proc->is_debug)
        klibc::debug_printf("Trying to pselect num_fds %d, read_set 0x%p, write_set 0x%p, except_set 0x%p, timeout 0x%p from proc %d\n",num_fds,read_set,write_set,except_set,timeout,proc->id);

    if(!is_safe_to_rw(proc, (std::uint64_t)read_set, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)write_set, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)except_set, PAGE_SIZE))
        return -EFAULT;

    if(!is_safe_to_rw(proc, (std::uint64_t)timeout, PAGE_SIZE))
        return -EFAULT;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
    pollfd fds[num_fds];
    klibc::memset(fds, 0, num_fds * sizeof(pollfd));
#pragma clang diagnostic pop
    
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
        num = poll_impl(fds, actual_count, (timeout->tv_sec * 1000) + (timeout->tv_nsec / (1000 * 1000)));
    } else {
        num = poll_impl(fds, actual_count, -1);
    }

    if(proc->is_debug)
        klibc::debug_printf("pselect6 to poll status %lli\n",num);

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

long long sys_seek(int fd, long offset, int whence) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -ESPIPE;

    switch (whence)
    {
        case SEEK_SET:
            file->offset = offset;
            break;

        case SEEK_CUR:
            file->offset += offset;
            break;

        case SEEK_END: {
            stat statz = {};
            int res = file->vnode.stat(file, &statz);
            if(res != 0)
                return res;

            file->offset = statz.st_size + offset;
            break;
        }

        default:
            return -EINVAL;
    }

    return file->offset;
}

long long sys_writev(int fd, iovec* vecs, std::uint64_t vlen) {
    thread* current = current_proc;
    std::uint64_t total = 0;
    if(!is_safe_to_rw(current, (std::uint64_t)vecs, (sizeof(iovec) * vlen) + PAGE_SIZE))
            return -EFAULT;
    for(std::uint64_t i = 0; i < vlen; i++) {
        if(!is_safe_to_rw(current, (std::uint64_t)vecs[i].iov_base, vecs[i].iov_len + PAGE_SIZE))
            return -EFAULT;
        long long ret = sys_write(fd, (char*)vecs[i].iov_base, vecs[i].iov_len);
        if(ret < 0)
            return ret;
        total += ret;
    }
    return total;
}

long long sys_getdents64(int fd, char* buf, std::uint64_t count) {
    thread* current = current_proc;

    if(!is_safe_to_rw(current, (std::uint64_t)buf, count + PAGE_SIZE)) 
        return -EFAULT;

    auto manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = nullptr;

    file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -EINVAL;

    if(buf == nullptr)
        return -EINVAL;

    if(file->vnode.ls == nullptr)
        return -ENOTSUP;

    return file->vnode.ls(file, buf, count);
}

#define STATX_BASIC_STATS 0x7ff
#define STATX_BTIME 0x800

long long sys_statx(int dfd, const char* path, int flags, std::uint32_t mask, statx* out) {
    (void)mask;
    thread* current = current_proc;

    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096) && path) {
        return -EFAULT;
    }

    if(!is_safe_to_rw(current, (std::uint64_t)out, 4096)) {
        return -EFAULT;
    }

    klibc::memset(out, 0, sizeof(stat));

    if(current->is_debug) {
        klibc::debug_printf("trying to statx %d %s 0x%p\n", dfd, path ? path : "empty path", flags);
    }

    bool is_empty = false;
    if(!path)
        is_empty = true;

    if(path) {
        if(path[0] == '\0')
            is_empty = true;
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor target_fd = {};
    if(is_empty) {
        file_descriptor* fd = manager->search(dfd);
        if(!fd)
            return -EBADF;
        target_fd = *fd;
    } else {
        char buffer1[4096] = {};
        char buffer[4096] = {};
        klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

        char* at = at_to_char(current, dfd);
        if(at == nullptr)
            return -EBADF;

        process_path(current->chroot, at, buffer1, buffer);

        target_fd.type = file_descriptor_type::file;
        int status = vfs::open(&target_fd, buffer, flags & AT_SYMLINK_NOFOLLOW ? false : true, false);
        if(status != 0)
            return status;

    }

    stat tmp_stat = {};

    if(target_fd.type != file_descriptor_type::file)
        return -EINVAL;

    assert(target_fd.vnode.stat, "no lol");

    target_fd.vnode.stat(&target_fd, &tmp_stat);

    if(!is_empty) {
        if(target_fd.vnode.close)
            target_fd.vnode.close(&target_fd);
    }

    // convert stat to statx

    klibc::memset(out, 0, sizeof(statx));

    out->stx_ino = tmp_stat.st_ino;
    out->stx_blksize = tmp_stat.st_blksize;
    out->stx_nlink = tmp_stat.st_nlink;
    out->stx_mode = tmp_stat.st_mode;
    out->stx_size = tmp_stat.st_size;
    out->stx_atime.tv_sec = tmp_stat.st_atim.tv_sec;
    out->stx_mtime.tv_sec = tmp_stat.st_mtim.tv_sec;
    out->stx_btime.tv_sec = tmp_stat.st_ctim.tv_sec;
    out->stx_blocks = tmp_stat.st_blocks;
    out->stx_uid = tmp_stat.st_uid;
    out->stx_gid = tmp_stat.st_gid;
    out->stx_mask = STATX_BASIC_STATS | STATX_BTIME;

    return 0;
}

long long sys_chdir(const char* path) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096)) {
        return -EFAULT;
    }

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current, AT_FDCWD);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, buffer);

    file_descriptor file = {};

    int status = vfs::open(&file, buffer, true, true);
    if(status != 0)
        return status;

    if(file.vnode.close) file.vnode.close(&file);

    klibc::debug_printf("chdir %s\n", buffer);

    klibc::memcpy(current->cwd, buffer, klibc::strlen(buffer) + 1);

    return 0;
}

long long sys_fchdir(int fd) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -EINVAL;

    stat tmp_stat = {};
    file->vnode.stat(file, &tmp_stat);

    if(!(tmp_stat.st_mode & S_IFDIR))
        return -ENOTDIR;

    klibc::debug_printf("fchdir %s (fd %d)\n", file->path, fd);
    klibc::memcpy(current->cwd, file->path, klibc::strlen(file->path) + 1);
    return 0;
}

long long sys_mkdir(const char* path, int mode) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096)) {
        return -EFAULT;
    }

    if(path == nullptr)
        return -EINVAL;

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current, AT_FDCWD);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, buffer);

    int status = vfs::create(buffer, vfs_file_type::directory, mode);
    if(status != 0)
        return status;

    return 0;
}

long long sys_mkdirat(int dfd, const char* path, int mode) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096)) {
        return -EFAULT;
    }

    if(path == nullptr)
        return -EINVAL;

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current, dfd);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, buffer);

    int status = vfs::create(buffer, vfs_file_type::directory, mode);
    if(status != 0)
        return status;

    return 0;
}

long long sys_umask(int mask) {
    (void)mask;
    return 0;
}

long long sys_close_range(int first, int last, int flags) {
    thread* current = current_proc;
    if(first > last)
        return -EINVAL;

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;

    if(flags & CLOSE_RANGE_UNSHARE) {
        if(manager->fd_usage_pointer > 1) {
            current->fd = new vfs::fdmanager;
            vfs::fdmanager* new_m = (vfs::fdmanager*)current->fd;
            manager->duplicate(new_m);
            manager->fd_usage_pointer--;
            manager = new_m;
        }
    }

    manager->close_range(first, last, (flags & CLOSE_RANGE_CLOEXEC) ? true : false);
    return 0;
}

long long sys_mount(const char* source, const char* target, const char* type, std::uint64_t mountflags, const void* data) {
    klibc::debug_printf("mount src %s target %s fstype %s flags 0x%p data 0x%p\n", source ? source : "no src", target ? target : "no target", type ? type : "no type", mountflags, data);
    return -ENOSYS;
}

long long sys_unlink(int dfd, const char* path, int flags) {
    (void)flags;
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096)) {
        return -EFAULT;
    }

    if(path == nullptr)
        return -EINVAL;

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current, dfd);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, buffer);

    int res = vfs::unlink(buffer);
    return res;
}

long long sys_unlink_path(const char* path) {
    return sys_unlink(AT_FDCWD, path, 0);
}

long long sys_chmod(const char* path, int mode) {
    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096)) {
        return -EFAULT;
    }

    if(path == nullptr)
        return -EINVAL;

    char buffer1[4096] = {};
    char buffer[4096] = {};
    klibc::memcpy(buffer1, path, safe_strlen((char*)path, 4096));

    char* at = at_to_char(current, AT_FDCWD);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, buffer);

    file_descriptor file = {};
    int res = vfs::open(&file, buffer, false, false);
    if(res != 0)
        return res;

    if(!file.vnode.chmod)
        return -ENOTSUP;

    file.vnode.chmod(&file, mode);

    return 0;
}

#define TIOCGWINSZ               0x5413

long long sys_ttyname(int fd, char *buf, std::size_t size) {
    thread* current = current_proc;

    winsize wz = {};
    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);

    if(!file)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -ENOTTY;

    if(!file->vnode.ioctl)
        return -ENOTTY;

    if(!is_safe_to_rw(current, (std::uint64_t)buf, size + PAGE_SIZE))
        return -EFAULT;

    int ret = file->vnode.ioctl(file, TIOCGWINSZ, (void*)&wz);

    if(ret != 0)
        return -ENOTTY;

    if((std::size_t)klibc::strlen(file->path) + 1 > size)
        return -ERANGE;

    klibc::memcpy(buf, file->path, klibc::strlen(file->path) + 1);

    return 0;
}