#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <generic/unix_sockets.hpp>
#include <generic/userspace/syscall_list.hpp>
#include <generic/userspace/sockets.hpp>
#include <generic/userspace/safety.hpp>
#include <generic/time.hpp>
#include <utils/signal_ret.hpp>
#include <utils/signal.hpp>
#include <generic/flock.hpp>

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
        int create_status = vfs::create(buffer, vfs_file_type::file, mode, current->uid, current->gid);
        if(create_status != 0)
            goto fail;
        status = vfs::open(new_fd, buffer, true, flags & O_DIRECTORY);
        if(status != 0)
            goto fail;
        assert(new_fd->vnode.chmod, "v");
        assert(new_fd->vnode.chown, "z");
        new_fd->vnode.chown(new_fd, current->uid, current->gid);
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
            klibc::debug_printf("TRUNC\n");
            if(new_fd->vnode.zero) {
                new_fd->vnode.zero(new_fd);
                ram_file::lock();
                ram_file::small(new_fd->inode, new_fd->vnode.fs, 0, nullptr);
                ram_file::unlock();
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

    bool is_empty = false;
    if(!path)
        is_empty = true;

    if(path) {
        if(path[0] == '\0')
            is_empty = true;
    }

    if(current->is_debug) {
        klibc::debug_printf("trying to stat %d %s 0x%p 0x%p\n", dfd, !is_empty ? path : "empty path", flags, path);
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

        sockaddr_un un = {};
        klibc::memcpy(un.sun_path, buffer, klibc::strlen(buffer) + 1);

        if(unix_sockets::is_exists(&un, false)) {
            klibc::debug_printf("socket\n");
            unix_socket_node* node = unix_sockets::find(&un);
            out->st_mode = node->mode | S_IFSOCK;
            return 0;
        }


        target_fd.type = file_descriptor_type::file;
        int status = vfs::open(&target_fd, buffer, (flags & AT_SYMLINK_NOFOLLOW) ? false : true, false);
        if(status != 0)
            return status;

    }

    if(target_fd.type == file_descriptor_type::pipe) {
        out->st_mode |= S_IFIFO;
        return 0;
    } else if(target_fd.type == file_descriptor_type::socket) {
        out->st_mode |= S_IFSOCK;
        return 0;
    }

    assert(target_fd.vnode.stat, "no lol");

    target_fd.vnode.stat(&target_fd, out);
    if(current->is_debug) klibc::debug_printf("mode %o %s size %lli", out->st_mode, path == nullptr ? "" : path, out->st_size);

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

long long sys_fstatfs(int fd, statfs* out) {
    thread* current_thread = current_proc;
    if(!is_safe_to_rw(current_thread, (std::uint64_t)out, PAGE_SIZE))
        return -EFAULT;

    auto manager = (vfs::fdmanager*)current_thread->fd;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    stat tmp_stat = {};
    file_descriptor target_fd = *file;

    target_fd.vnode.stat(&target_fd, &tmp_stat);

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

#define CAST_TO_PAGE(x) (ALIGNPAGEDOWN(x) / PAGE_SIZE)

long long read_page_cache(file_descriptor* file, char* buffer, std::size_t count, std::size_t file_size) {
    long actual_count = count > file_size - file->offset ? file_size - file->offset : count;

    if(file->offset > file_size)
        return 0;

    if(file_size - file->offset <= 0)
        return 0;

    assert(actual_count >= 0, "realy shit");

    if(actual_count == 0)
        return 0;

    ram_file::lock();
         
    std::size_t start_page = CAST_TO_PAGE(file->offset);
    std::size_t end_page = CAST_TO_PAGE(file->offset + actual_count - 1);

    long counter = actual_count;
    std::size_t ptr = file->offset;
    std::size_t memory_offset = 0;

    while(counter > 0) {

        std::size_t start_offset = 0;
        std::size_t memory_read = 0;
        std::size_t current_page = CAST_TO_PAGE(ptr);
        if(current_page == start_page && current_page != end_page) {
            start_offset = file->offset % PAGE_SIZE;
            memory_read = PAGE_SIZE - (file->offset % PAGE_SIZE);
            memory_read = (long)memory_read > actual_count ? actual_count : memory_read;
        } else if(current_page == end_page) {

            if(current_page == start_page)
                start_offset = file->offset % PAGE_SIZE;

            memory_read = counter;
        } else {
            memory_read = PAGE_SIZE;
            assert((long)memory_read < counter, "SHSIHHIHIIITTT");
        }

        ram_file::page* current = ram_file::access_page(file->inode, file->vnode.fs, current_page, false, nullptr, 0);

        if(current != nullptr) {
            klibc::memcpy(buffer + memory_offset, (void*)((std::uint64_t)current->p + start_offset), memory_read);
            file->offset += memory_read;
        } else {
            ram_file::unlock();
            std::size_t real_read = file->vnode.read(file, buffer + memory_offset, memory_read);
            assert(real_read == memory_read, "shit");
            ram_file::lock();
        }

        start_offset = 0;
        counter -= memory_read;
        ptr += memory_read;
        memory_offset += memory_read;
    }

    ram_file::unlock();
    return actual_count;
}

long long write_page_cache(file_descriptor* file, char* buffer, std::size_t count) {
    (void)file;
    (void)buffer;
    (void)count;
    assert(0, "SHGG");
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

        ram_file::lock();
        ram_file::content* file_ram = ram_file::get(file->inode, file->vnode.fs);
        ram_file::unlock();

        std::int64_t cz = 0;

        if(file_ram == nullptr)
            cz = file->vnode.read(file, buffer, count);
        else {
            stat stat_file = {};
            file->vnode.stat(file, &stat_file);
            cz = read_page_cache(file, buffer, count, stat_file.st_size);
        }

        if(cz == 0 && current->is_debug) {
            stat x = {};
            file->vnode.stat(file, &x);
            klibc::debug_printf("eof ! %s, seek %d, file_size %d", file->path, file->offset, x.st_size);
        }

        return cz;
    } else if(file->type == file_descriptor_type::pipe) {
        return file->fs_specific.pipe->read(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
    } else if(file->type == file_descriptor_type::socket) {
        if(file->socket.socket_type == PF_UNIX) {
            if(file->socket.read_socket == nullptr)
                return -ENOTCONN;

            if(file->socket.socket_side == 1) {

                if(current->is_debug) {
                    klibc::debug_printf("sock c %d %d", file->socket.write_socket->socket_counter.load(), file->socket.read_socket->size.load());
                }

                if(file->socket.write_socket->socket_counter == 0 && file->socket.read_socket->size.load() == 0)
                    return 0; 

                return file->socket.read_socket->read(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
            } else {

                if(current->is_debug) {
                    klibc::debug_printf("sock c %d %d", file->socket.read_socket->socket_counter.load(), file->socket.write_socket->size.load());
                }

                if(file->socket.read_socket->socket_counter == 0 && file->socket.write_socket->size.load() == 0)
                    return 0; 

                return file->socket.write_socket->read(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
            }
        }
    } else if(file->type == file_descriptor_type::socketpair) {
        if(file->socketpair.is_slave) {
            if(file->socketpair.write_socket->socket_counter == 0 && file->socketpair.read_socket->size.load() == 0)
                return 0; 

            return file->socketpair.read_socket->read(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
        } else {

            if(file->socketpair.read_socket->socket_counter == 0 && file->socketpair.write_socket->size.load() == 0)
                return 0; 

            return file->socketpair.write_socket->read(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
        }
    } else if(file->type == file_descriptor_type::eventfd) {

        if(count < 8)
            return -EINVAL;

        std::uint64_t current = file->eventfd.counter->load();
        std::uint64_t next = 0;

        // lock free impl

        while (true) {
            if (current == 0) {
                if (file->eventfd.flags & EFD_NONBLOCK) {
                    return -EAGAIN; 
                }
                
                current = file->eventfd.counter->load();
                continue;
            }

            if (file->eventfd.flags & EFD_SEMAPHORE) {
                next = current - 1;
            } else {
                next = 0;
            }

            if (file->eventfd.counter->compare_exchange_weak(current, next)) {
                break;
            }
            process::yield();
        }

        if (file->eventfd.flags & EFD_SEMAPHORE) {
            *(std::uint64_t*)buffer = 1; 
        } else {
            *(std::uint64_t*)buffer = current; 
        }

        return 8;
    }

    assert(0, "unimplemented read fd %d, type %d", fd, file->type);

    return -EFAULT;
}

long long sys_write(int fd, char* buffer, std::uint64_t count) {
    if(count == 0)
        return 0;

    thread* current = current_proc;
    thread* proc = current;

    if(buffer == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)buffer, count)) {
        return -EFAULT;
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    if(proc->is_debug) {
        klibc::debug_printf("trying to write %s fd %d buffer 0x%p type %d count %lli %s\n", file->path , fd, buffer, file->type, count, count > 1000 ? "too big" : buffer);
    }

    if(file->type == file_descriptor_type::file) {
        assert(ram_file::get(file->inode, file->vnode.fs) == nullptr, "shit");
        return file->vnode.write(file, buffer, count);
    } else if(file->type == file_descriptor_type::pipe) {
        return file->fs_specific.pipe->write(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
    } else if(file->type == file_descriptor_type::socket) {
        if(file->socket.socket_type == PF_UNIX) {
            if(file->socket.write_socket == nullptr)
                return -ENOTCONN;

            if(file->socket.socket_side == 1) {

                if(file->socket.write_socket->socket_counter == 0)
                    return -EPIPE; 

                return file->socket.write_socket->write(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
            } else {

                if(file->socket.read_socket->socket_counter == 0)
                    return -EPIPE; 

                return file->socket.read_socket->write(buffer, count, (file->flags & O_NONBLOCK) ? 1 : 0);
            }
        }
    } else if(file->type == file_descriptor_type::socketpair) {

        if(file->socketpair.is_slave) {
            file->socketpair.read_socket->sock_ucred.pid = proc->pid;
            file->socketpair.read_socket->sock_ucred.gid = proc->gid;
            file->socketpair.read_socket->sock_ucred.uid = proc->uid;
        } else {
            file->socketpair.write_socket->sock_ucred.pid = proc->pid;
            file->socketpair.write_socket->sock_ucred.gid = proc->gid;
            file->socketpair.write_socket->sock_ucred.uid = proc->uid;
        }

        if(file->socketpair.is_slave) {

            if(file->socketpair.write_socket->socket_counter == 0)
                return -EPIPE; 

            return file->socketpair.write_socket->write(buffer, count);
        } else {

            if(file->socketpair.read_socket->socket_counter == 0)
                return -EPIPE; 

            return file->socketpair.read_socket->write(buffer, count);
        }
    } else if(file->type == file_descriptor_type::eventfd) {
       
        if(count < 8)
            return -EINVAL;

        file->eventfd.counter->fetch_add(*(std::uint64_t*)buffer);
        return 8;
    }

    assert(0, "unimplemented write fd %d, type %d", fd, file->type);

    return -EFAULT;
}

long long sys_pwrite64(int fd, char* buffer, std::uint64_t count, std::uint64_t pos) {
    if(count == 0)
        return 0;

    thread* current = current_proc;

    if(buffer == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)buffer, count)) {
        return -EFAULT;
    }

    if(current->is_debug) {
        klibc::debug_printf("trying to pwrite64 fd %d buffer 0x%p count %lli pos %lli\n", fd, buffer, count, pos);
    }

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    std::uint64_t old_offset = file->offset;
    file->offset = pos;
    
    if(file->type == file_descriptor_type::file) {
        long long ret = file->vnode.write(file, buffer, count);
        file->offset = old_offset;
        return ret;
    } 

    return -ESPIPE;
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

    manager->close(file, current->pid);
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

    if(!is_safe_to_rw(current, arg, PAGE_SIZE))
        return -EFAULT;

    // FIONBIO
    if(req == 0x5421) {
        if((void*)arg == nullptr)
            return -EINVAL;

        int is_nonblock = *((int*)(arg));
        
        if(is_nonblock & 1) {
            file->flags |= O_NONBLOCK;
        } else {
            file->flags &= ~(O_NONBLOCK);
        }

        return 0;
    }

    if(file->type != file_descriptor_type::file)
        return -ENOTTY;

    if(!file->vnode.ioctl)
        return -ENOTTY;

    std::int32_t status = file->vnode.ioctl(file, req, (void*)arg);

    log("ioctl", "status %d", status);
    return status;

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

    // i dont have procfs so ill just do this
    if(klibc::strcmp("/proc/self/exe", buffer) == 0) {
        klibc::memcpy(buf, current->exe, klibc::strlen(current->exe) > bufsize ? bufsize : klibc::strlen(current->exe));
        return klibc::strlen(current->exe) > bufsize ? bufsize : klibc::strlen(current->exe);
    }

    int status = vfs::open(&target_fd, buffer, false, false);
    if(status != 0)
        return status;

    if(target_fd.type != file_descriptor_type::file)
        return -EINVAL;

    stat ss = {};
    target_fd.vnode.stat(&target_fd, &ss);

    if(!((ss.st_mode & S_IFMT) == S_IFLNK)) {
        if(target_fd.vnode.close)
            target_fd.vnode.close(&target_fd);

        return -EINVAL;
    }

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

    if(current->is_debug) {
        klibc::debug_printf("doing dup from %d to %d\n", fd, new_fd->index);
    }

    manager->do_dup(src, new_fd);
    return new_fd->index;
}

long long sys_dup2(int old, int new_fd) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;
    file_descriptor* src = manager->search(old);
    if(src == nullptr)
        return -EBADF;

    if(current->is_debug) {
        klibc::debug_printf("doing dup2 from %d to %d\n", old, new_fd);
    }

    file_descriptor* new_fd_s = manager->try_dup2(src, new_fd);
    return new_fd_s->index;
}

long long sys_fcntl(int fd, int request, std::uint64_t arg) {

    thread* proc = current_proc;
    auto manager = (vfs::fdmanager*)proc->fd;

    if(!is_safe_to_rw(proc, arg, PAGE_SIZE))
        return -EFAULT;

    if(1)
        klibc::debug_printf("fcntl fd %d req %d arg 0x%p from proc %d",fd,request,arg);
    int is_cloexec = 0;
    switch(request) {
        case F_DUPFD_CLOEXEC:
            is_cloexec = 1;
        case F_DUPFD: {
            file_descriptor* fd_s = manager->search(fd);
            if(fd_s == nullptr)
                return -EBADF;
            file_descriptor* new_fd = manager->createlowest((std::int64_t)arg - 1);
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

            return fd_s->flags;
        }

        case F_SETFL: {
            file_descriptor* fd_s = manager->search(fd);

            if(!fd_s)
                return -EBADF;

            fd_s->flags &= ~(O_APPEND | O_ASYNC | O_NONBLOCK | O_RDONLY | O_RDWR | O_WRONLY);
            fd_s->flags |= (arg & (O_APPEND | O_ASYNC | O_NONBLOCK | O_RDONLY | O_RDWR | O_WRONLY));

            if(fd_s->flags & O_NONBLOCK) {
                fd_s->eventfd.flags |= EFD_NONBLOCK;
            } else {
                fd_s->eventfd.flags &= ~(EFD_NONBLOCK);
            }

            return 0;
        }

        // my analog of F_GETPATH 
        case 0x10209040: {
            file_descriptor* fd_s = manager->search(fd);

            if(!fd_s)
                return -EBADF;

            klibc::memcpy((void*)arg, fd_s->path, klibc::strlen(fd_s->path) + 1);
            return 0;
        }

        // F_GETLK
        case 5: {

            file_descriptor* fd_s = manager->search(fd);

            if(!fd_s)
                return -EBADF;

            if(fd_s->type != file_descriptor_type::file)
                return -EINVAL;

            stat file_stat = {};
            fd_s->vnode.stat(fd_s, &file_stat);

            flock::flock_struct* flock_user = (flock::flock_struct*)arg;
            
            fd_s->vnode.fs->flock_related.lock.lock();
            flock::flock_struct* lock = flock::search(fd_s->vnode.fs, fd_s->inode, flock_user->l_start, flock_user->l_len, flock_user->l_whence, fd_s->offset, file_stat.st_size);

            if(lock == nullptr) {
                klibc::memset(flock_user, 0, sizeof(flock::flock_struct));
                flock_user->l_type = F_UNLCK;
                fd_s->vnode.fs->flock_related.lock.unlock();
                return 0;
            } 

            *flock_user = *lock;

            fd_s->vnode.fs->flock_related.lock.unlock();

            return 0;
        }
        
        // F_SETLK
        case 6: {
        
            file_descriptor* fd_s = manager->search(fd);

            if(!fd_s)
                return -EBADF;

            if(fd_s->type != file_descriptor_type::file)
                return -EINVAL;

            if(arg == (std::uintptr_t)nullptr)
                return -EINVAL;

            stat file_stat = {};
            fd_s->vnode.stat(fd_s, &file_stat);

            flock::flock_struct* flock_user = (flock::flock_struct*)arg;
            
            if(flock_user->l_type != F_UNLCK) {
                flock::flock_struct* lock = flock::create(fd_s->vnode.fs, fd_s->inode, flock_user->l_start, flock_user->l_len, flock_user->l_type, flock_user->l_whence, fd_s->offset, file_stat.st_size, proc->pid);

                if(lock == nullptr)
                    return -EAGAIN;

                lock->l_pid = proc->pid;
                return 0;

            } else {
                fd_s->vnode.fs->flock_related.lock.lock();
                flock::flock_struct* lock = flock::search(fd_s->vnode.fs, fd_s->inode, flock_user->l_start, flock_user->l_len, flock_user->l_whence, fd_s->offset, file_stat.st_size);
                
                if(lock == nullptr) {
                    fd_s->vnode.fs->flock_related.lock.unlock();
                    return 0;
                }
                
                if((std::uint32_t)lock->l_pid != proc->pid) {
                    fd_s->vnode.fs->flock_related.lock.unlock();
                    return -EAGAIN;
                }

                lock->l_type = F_UNLCK;
                fd_s->vnode.fs->flock_related.lock.unlock();
                return 0;

            }
            
            return 0;
        }

        default: {
            return -EINVAL;
            assert(0,"unsupported fcntl fd %d req %d arg 0x%p from proc %d",fd,request,arg);
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

    klibc::debug_printf("creating pipe %d-%d\n", fds[0], fds[1]);

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
        if(fd == nullptr) {
            fds[i].events = 0;
            fds[i].revents = 0;
            fds[i].fd = 0;
            cached[i] = nullptr;
            klibc::debug_printf("null fd poll ignoring\n");
            continue;
        }

        cached[i] = fd;

        if(current->is_debug) {
            char type[64] = {};
            poll_to_str(fds[i].events, type);
            klibc::debug_printf("trying to poll fd %d, type %s (%d), fd type %d socket_type %d, socket_specific 0x%p un_write 0x%p un_read 0x%p socket_pointer 0x%p socket_side %d\n", fds[i].fd, type ,fds[i].events, fd->type, fd->socket.socket_type, fd->socket.socket_specific, fd->socket.write_socket, fd->socket.read_socket, fd->socket.socket_pointer, fd->socket.socket_side);
        }
    }

    auto poll_body = [fds, nfds, current](file_descriptor** cached) -> int {
        int count = 0;
        for(std::uint32_t i = 0;i < nfds;i++) {

            if(cached[i] == nullptr)
                continue;

            file_descriptor* fd = cached[i];
            bool is_event = false;
            if(fds[i].events & POLLIN) {
                bool ret = false;
                if(fd->type == file_descriptor_type::file) {
                    if(fd->vnode.poll) {
                        ret = fd->vnode.poll(fd, vfs_poll_type::pollin);
                    } else {
                        log("poll", "there's no poll for file %s", fd->path);
                    }
                } else if(fd->type == file_descriptor_type::pipe) {
                    if(fd->fs_specific.pipe->size.load() != 0) 
                        ret = true;
                } else if(fd->type == file_descriptor_type::socket && fd->socket.is_listen) {
                    if(fd->socket.socket_type == PF_UNIX) {
                        auto node = (unix_socket_node*)fd->socket.socket_pointer;
                        if(node->conn_counter > 0)
                            ret = true;
                    }
                } else if(fd->type == file_descriptor_type::socket && !fd->socket.is_listen) {
                    if(fd->socket.socket_type ==  PF_UNIX && fd->socket.write_socket != nullptr && fd->socket.read_socket != nullptr) {
                        if(fd->socket.socket_side == 1) {
                            if(fd->socket.read_socket->size.load() != 0)
                                ret = true;
                        } else {
                            if(fd->socket.write_socket->size.load() != 0)
                                ret = true;
                        }
                    }
                } else if(fd->type == file_descriptor_type::socketpair) {
                    if(fd->socketpair.is_slave) {
                        if(fd->socketpair.read_socket->size.load() != 0)
                            ret = true;
                    } else {
                        if(fd->socketpair.write_socket->size.load() != 0)
                            ret = true;
                    } 
                } else if(fd->type == file_descriptor_type::eventfd) {
                    if(fd->eventfd.counter->load() > 0)
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
                    } else {
                        log("poll", "there's no poll for file %s", fd->path);
                    }
                } else if(fd->type == file_descriptor_type::pipe) {
                    if((std::uint64_t)fd->fs_specific.pipe->size.load() != fd->fs_specific.pipe->total_size) 
                        ret = true;
                } else if(fd->type == file_descriptor_type::socket && !fd->socket.is_listen) {
                    if(fd->socket.socket_type ==  PF_UNIX && fd->socket.write_socket != nullptr && fd->socket.read_socket != nullptr) {
                        if(fd->socket.socket_side == 1) {
                            klibc::debug_printf("meoww1 %lli %lli\n", fd->socket.write_socket->size.load(), fd->socket.write_socket->total_size);
                            if((std::uint64_t)fd->socket.write_socket->size.load() != fd->socket.write_socket->total_size)
                                ret = true;
                        } else {
                            klibc::debug_printf("meoww2 %lli %lli\n", fd->socket.read_socket->size.load(),  fd->socket.read_socket->total_size);
                            if((std::uint64_t)fd->socket.read_socket->size.load() != fd->socket.read_socket->total_size)
                                ret = true;
                        }
                    }
                } else if(fd->type == file_descriptor_type::socketpair) {
                    if(fd->socketpair.is_slave) {
                        if((std::uint64_t)fd->socketpair.write_socket->size.load() != fd->socketpair.write_socket->total_size)
                            ret = true;
                    } else {
                        if((std::uint64_t)fd->socketpair.read_socket->size.load() != fd->socketpair.read_socket->total_size)
                            ret = true;
                    }
                } else if(fd->type == file_descriptor_type::eventfd) {
                    ret = true;
                }

                if(ret == true) {
                    fds[i].revents |= POLLOUT;
                    is_event = true;
                }
            }

            bool pollhup_ret = false;

            if(fd->type == file_descriptor_type::socket && !fd->socket.is_listen) {
                if(fd->socket.socket_side == PF_UNIX && fd->socket.write_socket != nullptr && fd->socket.read_socket != nullptr) {
                    if(fd->socket.socket_side == 1) {
                        if(fd->socket.write_socket->connected_to_pipe_write == 0 && fd->socket.write_socket->size != 0)
                            pollhup_ret = true;
                    } else {
                        if(fd->socket.read_socket->connected_to_pipe_write == 0 && fd->socket.read_socket->size != 0)
                            pollhup_ret = true;
                    }
                }
            }

            if(fd->type == file_descriptor_type::socketpair) {
                if(fd->socketpair.is_slave) {
                    if(fd->socketpair.write_socket->connected_to_pipe_write == 0 && fd->socketpair.write_socket->size != 0)
                        pollhup_ret = true;
                } else {
                    if(fd->socketpair.read_socket->connected_to_pipe_write == 0 && fd->socketpair.read_socket->size != 0)
                        pollhup_ret = true;
                }
            }

            if(fd->type == file_descriptor_type::pipe) {
                if(fd->fs_specific.pipe->connected_to_pipe_write == 0)
                    pollhup_ret = true;
            }

            if(pollhup_ret) {
                fds[i].revents |= POLLHUP;
                is_event = true;
            }

            if(is_event)
                count++;

            if(is_event && current->is_debug) {
                klibc::debug_printf("poll fd %d revents %d events %d", fds[i].fd, fds[i].revents, fds[i].events);
            }
        }
        return count;
    };

    if(timeout == -1) {
        while(true) {
            int ret = poll_body(cached);
            if(ret != 0)
                return ret;

            is_signal_ret(current) {
                signal_ret(current);
            }

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

                is_signal_ret(current) {
                    signal_ret(current);
                }

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

    if(!is_safe_to_rw(proc, (std::uint64_t)sigmask, PAGE_SIZE))
        return -EFAULT;

    if(sigmask != nullptr) {
        proc->is_restore_sigset = true;
        proc->temp_sigset = proc->sigset;
        proc->sigset = *sigmask;
    }

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

#define EPOLL_CLOEXEC 02000000
#define EPOLLIN 0x001
#define EPOLLPRI 0x002
#define EPOLLOUT 0x004
#define EPOLLRDNORM 0x040
#define EPOLLRDBAND 0x080
#define EPOLLWRNORM 0x100
#define EPOLLWRBAND 0x200
#define EPOLLMSG 0x400
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLLRDHUP 0x2000
#define EPOLLEXCLUSIVE (1U << 28)
#define EPOLLWAKEUP (1U << 29)
#define EPOLLONESHOT (1U << 30)
#define EPOLLET (1U << 31)
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

long long sys_epoll_create(int flags) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    file_descriptor* fd0 = manager->createlowest(2);
    fd0->type = file_descriptor_type::epoll;
    fd0->other.is_cloexec = (flags & EPOLL_CLOEXEC) ? true : false; 
    fd0->epoll.epoll_usage_counter = new std::atomic<std::uint32_t>;
    fd0->epoll.epoll_usage_counter->store(1);
    fd0->epoll.info = new epoll_member*;
    fd0->epoll.epoll_lock = new locks::spinlock;
    fd0->epoll.epoll_ptr = new std::size_t;

    fd0->epoll.epoll_lock->unlock();
    *fd0->epoll.epoll_ptr = 0;

    klibc::debug_printf("creating epoll %d\n", fd0->index);

    return fd0->index;
}

long long sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev) {
    
    thread* current = current_proc;

    if(ev == nullptr && op != EPOLL_CTL_DEL)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)ev, PAGE_SIZE))
        return -EFAULT;

    auto manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);
    file_descriptor* epoll = manager->search(epfd);

    if(epoll == nullptr)
        return -EBADF;

    if(file == nullptr)
        return -EBADF;

    if(epoll->type != file_descriptor_type::epoll)
        return -EINVAL;

    klibc::debug_printf("epollctl epfd %d op %d fd %d ev 0x%p 0x%p 0x%p\n", epfd, op, fd, ev, epoll->epoll.epoll_lock, epoll->epoll.info);

    epoll->epoll.epoll_lock->lock();

    epoll_member* info = *epoll->epoll.info;

    switch(op) {
        case EPOLL_CTL_ADD: {

            if(info != nullptr) {
                for(std::size_t i = 0; i < *epoll->epoll.epoll_ptr; i++) {
                    if(info[i].is_used == true && info[i].target_fd == fd) {
                        epoll->epoll.epoll_lock->unlock();
                        return -EEXIST;
                    }
                }
            }

            epoll_member* free_epoll = nullptr;
            if(info != nullptr) {
                for(std::size_t i = 0; i < *epoll->epoll.epoll_ptr; i++) {
                    if(info[i].is_used == false) {
                        free_epoll = &info[i];
                        goto end;
                    }
                }
            }
end:
            // increase/create epoll array
            if(free_epoll == nullptr) {
                *epoll->epoll.epoll_ptr = *epoll->epoll.epoll_ptr + 1;
                epoll_member* new_info = (epoll_member*)klibc::malloc(*epoll->epoll.epoll_ptr * sizeof(epoll_member));

                klibc::memset(new_info, 0, *epoll->epoll.epoll_ptr * sizeof(epoll_member));

                if(info != nullptr) {
                    klibc::memcpy(new_info, info, (*epoll->epoll.epoll_ptr - 1) * sizeof(epoll_member));
                    klibc::free((void*)info); 
                }

                *epoll->epoll.info = new_info;
                info = new_info;

                free_epoll = &info[*epoll->epoll.epoll_ptr - 1];
            }

            free_epoll->is_used = true;
            free_epoll->target_fd = fd;
            free_epoll->ev = *ev;

            epoll->epoll.epoll_lock->unlock();
            return 0;
        }

        case EPOLL_CTL_DEL: {
            if(info != nullptr) {
                for(std::size_t i = 0; i < *epoll->epoll.epoll_ptr; i++) {
                    if(info[i].is_used == true && info[i].target_fd == fd) {
                        info[i].is_used = false;
                        epoll->epoll.epoll_lock->unlock();
                        return 0;
                    }
                }
            }

            epoll->epoll.epoll_lock->unlock();
            return -ENOENT;
        }

        case EPOLL_CTL_MOD: {
            if(info != nullptr) {
                for(std::size_t i = 0; i < *epoll->epoll.epoll_ptr; i++) {
                    if(info[i].is_used == true && info[i].target_fd == fd) {
                        info[i].ev = *ev;
                        epoll->epoll.epoll_lock->unlock();
                        return 0;
                    }
                }
            }

            epoll->epoll.epoll_lock->unlock();
            return -ENOENT;
        }

        default:
            epoll->epoll.epoll_lock->unlock();
            return -EINVAL;
    }

    assert(0, "huh");
    return -EFAULT;
}

inline static int epoll_to_poll_events(int events) {
    int result = 0;

    if(events & EPOLLIN)
        result |= POLLIN;

    if(events & EPOLLOUT)
        result |= POLLOUT;

    if(events & EPOLLHUP)
        result |= POLLHUP;

    if(events & EPOLLRDHUP)
        result |= POLLHUP;

    return result;
}

inline static int poll_to_epoll_events(int events) {
    int result = 0;

    if(events & POLLIN)
        result |= EPOLLIN;

    if(events & POLLOUT)
        result |= EPOLLOUT;

    if(events & POLLHUP)
        result |= EPOLLHUP;

    return result;
}

long long sys_epoll_wait(int epfd, struct epoll_event *ev, int n, int timeout, const sigset_t *sigmask) {
    thread* current = current_proc;

    if(n <= 0)
        return 0;

    if(ev == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)ev, PAGE_SIZE + (n * sizeof(epoll_event))))
        return -EFAULT;

    if(!is_safe_to_rw(current, (std::uint64_t)sigmask, PAGE_SIZE))
        return -EFAULT;

    if(sigmask != nullptr) {
        current->is_restore_sigset = true;
        current->temp_sigset = current->sigset;
        current->sigset = *sigmask;
    }

    auto manager = (vfs::fdmanager*)current->fd;
    file_descriptor* epoll = manager->search(epfd);

    if(epoll == nullptr)
        return -EBADF;

    if(epoll->type != file_descriptor_type::epoll)
        return -EINVAL;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-cxx-extension"

    std::size_t ptr = *epoll->epoll.epoll_ptr;
    epoll_member* info = *epoll->epoll.info;

    struct pollfd converted[ptr];
    klibc::memset(converted, 0, ptr * sizeof(pollfd));

    for(std::size_t i = 0; i < ptr; i++) {
        if(info[i].is_used == true) {
            converted[i].fd = info[i].target_fd;
            converted[i].events = epoll_to_poll_events(info[i].ev.events);
        } else {
            converted[i].fd = -1;
            converted[i].events = 0;
        }
    }

    long long ret = poll_impl(converted, ptr, timeout);

    std::size_t proc_events = 0;

    for(std::size_t i = 0; i < ptr; i++) {
        if(proc_events >= (std::size_t)n)
            break;

        if(converted[i].revents != 0 && info[i].is_used) {
            ev[proc_events] = info[i].ev;
            ev[proc_events].events = poll_to_epoll_events(converted[i].revents);
            proc_events++;
        }
    }

    if(ret < 0)
        return ret;

#pragma clang diagnostic pop
    
    return proc_events;
}

long long sys_seek(int fd, long offset, int whence) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    klibc::debug_printf("seek fd %d offset %d whence %d\n", fd, offset, whence);

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
            if(file->offset < 0)
                file->offset = 0;
            break;
        }

        default:
            return -EINVAL;
    }

    assert(file->offset >= 0, "wtf fd %d offset %lli whence %d", fd, offset, whence);
    return file->offset;
}

long long sys_writev(int fd, iovec* vecs, std::uint64_t vlen) {
    thread* current = current_proc;
    std::uint64_t total = 0;
    klibc::debug_printf("writev fd %d vecs 0x%p vlen %lli\n", fd, vecs, vlen);
    if(!is_safe_to_rw(current, (std::uint64_t)vecs, (sizeof(iovec) * vlen) + PAGE_SIZE))
            return -EFAULT;
    for(std::uint64_t i = 0; i < vlen; i++) {
        if(!is_safe_to_rw(current, (std::uint64_t)vecs[i].iov_base, vecs[i].iov_len + PAGE_SIZE))
            return -EFAULT;
        long long ret = sys_write(fd, (char*)vecs[i].iov_base, vecs[i].iov_len);
        if(ret < 0 && ret != -EAGAIN)
            return ret;
        else if(ret == -EAGAIN)
            return total;
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

    assert(target_fd.vnode.stat, "no lol %s", target_fd.path);

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

    if((tmp_stat.st_mode & S_IFMT) != S_IFDIR)
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

    int status = vfs::create(buffer, vfs_file_type::directory, mode, current->uid, current->gid);
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

    int status = vfs::create(buffer, vfs_file_type::directory, mode, current->uid, current->gid);
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

    // unix socket test
    sockaddr_un un = {};
    klibc::memcpy(un.sun_path, buffer, klibc::strlen(buffer) + 1);

    klibc::debug_printf("trying to unlink %s\n", buffer);
    if(unix_sockets::is_exists(&un, false)) {
        klibc::debug_printf("socket\n");
        unix_socket_node* node = unix_sockets::find(&un);
        klibc::memset(&node->path, 0 , sizeof(node->path)); 
        process_close(node);
        return 0;
    }

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

    // unix socket test
    sockaddr_un un = {};
    klibc::memcpy(un.sun_path, buffer, klibc::strlen(buffer) + 1);

    klibc::debug_printf("trying to chmod %s with mdoe %d\n", buffer, mode);
    if(unix_sockets::is_exists(&un, false)) {
        klibc::debug_printf("socket\n");
        unix_socket_node* node = unix_sockets::find(&un);
        node->mode = mode & ~(S_IFBLK | S_IFCHR | S_IFDIR | S_IFREG | S_IFIFO);
        return 0;
    }

    file_descriptor file = {};
    int res = vfs::open(&file, buffer, false, false);
    if(res != 0)
        return res;

    if(!file.vnode.chmod)
        return -ENOTSUP;

    file.vnode.chmod(&file, mode);

    return 0;
}

long long sys_fchmod(int fd, mode_t mode) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    file_descriptor* file = manager->search(fd);
    if(file == nullptr)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -EINVAL;

    klibc::debug_printf("fchmod %s (fd %d)\n", file->path, fd);
    
    long long ret = 0;
    if(file->vnode.chmod) {
        ret = file->vnode.chmod(file, mode);
    }

    return ret;
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

// hard links are unimplemented so just convert to symlinks 
long long sys_linkat(int olddirfd, const char *old_path, int newdirfd, const char *new_path, int flags) {
    (void)flags;

    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)old_path, 4096)) {
        return -EFAULT;
    }

    if(old_path == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)new_path, 4096)) {
        return -EFAULT;
    }

    if(new_path == nullptr)
        return -EINVAL;

    char buffer1[4096] = {};
    char old_path1[4096] = {};
    klibc::memcpy(buffer1, old_path, safe_strlen((char*)old_path, 4096));

    char* at = at_to_char(current, olddirfd);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, old_path1);

    char buffer12[4096] = {};
    char new_path1[4096] = {};
    klibc::memcpy(buffer12, new_path, safe_strlen((char*)new_path, 4096));

    at = at_to_char(current, newdirfd);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer12, new_path1);

    char tmp[4096] = {};

    if(vfs::readlink(new_path1, tmp, 4096) != -ENOENT)
        return -EEXIST;

    if(vfs::readlink(old_path1, tmp, 4096) == ENOENT)
        return -ENOENT;

    return vfs::link(old_path1, new_path1, (flags & AT_SYMLINK_NOFOLLOW) ? false : true);
}

long long sys_link(const char *old_path, const char *new_path) {
    return sys_linkat(AT_FDCWD, old_path, AT_FDCWD, new_path, 0);
}  

long long sys_renameat(int olddirfd, const char *old_path, int newdirfd, const char *new_path) {
    int flags = 0;

    thread* current = current_proc;
    if(!is_safe_to_rw(current, (std::uint64_t)old_path, 4096)) {
        return -EFAULT;
    }

    if(old_path == nullptr)
        return -EINVAL;

    if(!is_safe_to_rw(current, (std::uint64_t)new_path, 4096)) {
        return -EFAULT;
    }

    if(new_path == nullptr)
        return -EINVAL;

    char buffer1[4096] = {};
    char old_path1[4096] = {};
    klibc::memcpy(buffer1, old_path, safe_strlen((char*)old_path, 4096));

    char* at = at_to_char(current, olddirfd);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer1, old_path1);

    char buffer12[4096] = {};
    char new_path1[4096] = {};
    klibc::memcpy(buffer12, new_path, safe_strlen((char*)new_path, 4096));

    at = at_to_char(current, newdirfd);
    if(at == nullptr)
        return -EBADF;

    process_path(current->chroot, at, buffer12, new_path1);

    char tmp[4096] = {};

    if(vfs::readlink(new_path1, tmp, 4096) != -ENOENT)
        return -EEXIST;

    if(vfs::readlink(old_path1, tmp, 4096) == ENOENT)
        return -ENOENT;

    return vfs::rename(old_path1, new_path1, (flags & AT_SYMLINK_NOFOLLOW) ? false : true);
}

long long sys_rename(const char *old_path, const char *new_path) {
    return sys_renameat(AT_FDCWD, old_path, AT_FDCWD, new_path);
}  

long long sys_ptsname(int fd, char *buffer, size_t length) {
    thread* current = current_proc;

    vfs::fdmanager* manager = (vfs::fdmanager*)current->fd;
    file_descriptor* file = manager->search(fd);

    if(!file)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -ENOTTY;

    if(!file->vnode.ioctl)
        return -ENOTTY;

    if(!is_safe_to_rw(current, (std::uint64_t)buffer, length + PAGE_SIZE))
        return -EFAULT;

    char built[4096] = {};
    klibc::__printfbuf(built, 4096, "/dev/pts/%d", file->other.tty_num);
    if((std::size_t)klibc::strlen(built) + 1 > length)
        return -ERANGE;

    klibc::memcpy(buffer, built, klibc::strlen(built) + 1);

    return 0;
}

long long sys_ftruncate(int fd, std::size_t new_size) {
    thread* current = current_proc;

    auto manager = (vfs::fdmanager*)current->fd;
    auto file = manager->search(fd);

    if(file == nullptr)
        return -EBADF;

    if(file->type != file_descriptor_type::file)
        return -EINVAL;

    if(file->vnode.truncate == nullptr)
        return -ENOTSUP;

    klibc::debug_printf("trunc file %s to size %lli\n", file->path, new_size);

    file->vnode.truncate(file, new_size);

    ram_file::lock();
    ram_file::small(file->inode, file->vnode.fs, new_size == 0 ? 0 : ALIGNPAGEUP(new_size) / PAGE_SIZE, nullptr);

    ram_file::content* file_ram = ram_file::get(file->inode, file->vnode.fs);

    if(file_ram != nullptr) {
        
        std::size_t end_page = CAST_TO_PAGE(new_size);
        ram_file::page* END = ram_file::access_page(file->inode, file->vnode.fs, end_page, false, nullptr, 0);

        if(END != nullptr) {
            void* dest = (void*)((std::uint64_t)END->p + (new_size % PAGE_SIZE));
            klibc::memset(dest, 0, PAGE_SIZE - (new_size % PAGE_SIZE));
        }

    }

    ram_file::unlock();

    return 0;
}

long long sys_fchownat(int dfd, const char* path, uid_t owner, gid_t group, int flags) {
    thread* current = current_proc;

    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096) && path) {
        return -EFAULT;
    }

    bool is_empty = false;
    if(!path)
        is_empty = true;

    if(path) {
        if(path[0] == '\0')
            is_empty = true;
    }

    if(current->is_debug) {
        klibc::debug_printf("trying to chown %d %s 0x%p 0x%p with uid %d gid %d\n", dfd, !is_empty ? path : "empty path", flags, path, owner, group);
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

        sockaddr_un un = {};
        klibc::memcpy(un.sun_path, buffer, klibc::strlen(buffer) + 1);

        if(unix_sockets::is_exists(&un, false)) {
            return 0;
        }

        target_fd.type = file_descriptor_type::file;
        int status = vfs::open(&target_fd, buffer, (flags & AT_SYMLINK_NOFOLLOW) ? false : true, false);
        if(status != 0)
            return status;

    }

    assert(target_fd.vnode.chown, "no lol chown");

    target_fd.vnode.chown(&target_fd, owner, group);

    if(!is_empty) {
        if(target_fd.vnode.close)
            target_fd.vnode.close(&target_fd);
    }

    return 0;
}

long long sys_fsync(int fd) {
    klibc::debug_printf("fsync was called on fd %d\n", fd);
    return 0;
}

long long sys_eventfd_create(std::uint64_t initval, int flags) {
    thread* current = current_proc;
    auto manager = (vfs::fdmanager*)current->fd;

    file_descriptor* fd0 = manager->createlowest(2);
    fd0->type = file_descriptor_type::eventfd;
    fd0->eventfd.counter = new std::atomic<std::uint64_t>;
    fd0->eventfd.ref_count = new std::atomic<std::size_t>;
    fd0->eventfd.ref_count->store(1);
    fd0->eventfd.flags = flags;
    fd0->other.is_cloexec = (flags & EFD_CLOEXEC) ? true : false;

    fd0->eventfd.counter->store(initval);

    klibc::debug_printf("creating eventfd %d\n", fd0->index);

    return fd0->index;
}