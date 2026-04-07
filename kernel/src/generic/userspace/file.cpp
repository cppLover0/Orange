#include <generic/userspace/syscall_list.hpp>
#include <generic/pmm.hpp>
#include <generic/vfs.hpp>
#include <generic/vmm.hpp>
#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <utils/errno.hpp>
#include <generic/userspace/syscall_list.hpp>
#include <generic/userspace/safety.hpp>

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

    assert((std::uint64_t)(klibc::strlen(buffer1) + 1) < sizeof(new_fd->path), "bruh.");
    klibc::memcpy(new_fd->path, buffer1, klibc::strlen(buffer1) + 1);

    new_fd->flags = flags;

    return new_fd->index;
}

long long sys_newfstatat(int dfd, const char* path, stat* out, int flags) {
    thread* current = current_proc;

    if(!is_safe_to_rw(current, (std::uint64_t)path, 4096) && path) {
        return -EFAULT;
    }

    if(!is_safe_to_rw(current, (std::uint64_t)out, 4096)) {
        return -EFAULT;
    }

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

        int status = vfs::open(&target_fd, buffer, flags & AT_SYMLINK_NOFOLLOW ? false : true, false);
        if(status != 0)
            return status;

    }

    if(target_fd.type != file_descriptor_type::file)
        return -EINVAL;

    assert(target_fd.vnode.stat, "no lol");

    target_fd.vnode.stat(&target_fd, out);

    return 0;
}

long long sys_fstat(int fd, stat* out) {
    return sys_newfstatat(fd, nullptr, out, 0);
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
        return file->fs_specific.pipe->read(buffer, count, file->other.is_non_block);
    }

    assert(0, "unimplemented read fd %d, type %d", fd, file->type);

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