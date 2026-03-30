#include <generic/vfs.hpp>
#include <generic/devfs.hpp>
#include <klibc/string.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <utils/errno.hpp>
#include <generic/lock/spinlock.hpp>
#include <cstdint>
#include <atomic>

devfs_node* head_devfs_node = nullptr;
devfs_node root_devfs_node = {};

std::atomic<std::uint64_t> devfs_id = 0;

locks::preempt_spinlock devfs_lock;

devfs_node* devfs_lookup(char* path) {

    if(klibc::strcmp(path,"/\0") == 0)
        return &root_devfs_node;

    bool state = devfs_lock.lock();
    devfs_node* current_node = head_devfs_node;
    while(current_node != nullptr) {
        if(klibc::strcmp(current_node->path, path) == 0) {
            devfs_lock.unlock(state);
            return current_node;
        }
    }
    devfs_lock.unlock(state);
    return nullptr;
}

void create(bool is_tty, char* path, void* arg, std::uint64_t mmap, std::uint64_t mmap_size, std::int32_t (*open)(file_descriptor*fd, devfs_node* node), std::int32_t (*ioctl)(devfs_node* node, std::uint64_t req, void* arg), signed long (*read)(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count), signed long (*write)(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count), bool (*poll)(devfs_node* node, vfs_poll_type type), std::int32_t (*close)(file_descriptor* fd, devfs_node* node)) {
    bool state = devfs_lock.lock();

    assert(devfs_lookup(path) == nullptr," afdsfsdfsf!!!");

    devfs_node* new_node = (devfs_node*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_node->mmap = mmap;
    new_node->mmap_size = mmap_size;
    new_node->open = open;
    new_node->write = write;
    new_node->read = read;
    new_node->poll = poll;
    new_node->close = close;
    new_node->ioctl = ioctl;
    new_node->arg = arg;
    new_node->is_a_tty = is_tty;
    new_node->id = ++devfs_id;

    new_node->next = head_devfs_node;
    head_devfs_node = new_node;

    devfs_lock.unlock(state);
}

std::int32_t devfs_ioctl(file_descriptor* file, std::uint64_t req, void* arg) {
    auto node = (devfs_node*)file->fs_specific.tmpfs_pointer;
    if(node->ioctl == nullptr)
        return -ENOTSUP;
    return node->ioctl(node, req, arg);
}

signed long devfs_read(file_descriptor* fd, void* buffer, std::size_t count) {
    auto node = (devfs_node*)fd->fs_specific.tmpfs_pointer;
    if(node->read == nullptr)
        return -ENOTSUP;
    return node->read(fd, node, buffer, count);
}

signed long devfs_write(file_descriptor* fd, void* buffer, std::size_t count) {
    auto node = (devfs_node*)fd->fs_specific.tmpfs_pointer;
    if(node->write == nullptr)
        return -ENOTSUP;
    return node->write(fd, node, buffer, count);
}

std::int32_t devfs_stat(file_descriptor* file, stat* out) {
    if(((devfs_node*)file->other.ls_pointer)->is_root == true) {
        out->st_mode = S_IFDIR | 0666;
        return 0;
    }
    (void)file;
    (void)out;
    out->st_mode = S_IFCHR | 0666;
    return 0;
}

bool devfs_poll(file_descriptor* file, vfs_poll_type type) {
    auto node = (devfs_node*)file->fs_specific.tmpfs_pointer;
    if(node->poll == nullptr)
        return false;
    return node->poll(node, type);
}

std::int32_t devfs_mmap(file_descriptor* file, std::uint64_t* out_phys, std::size_t* out_size) {
    auto node = (devfs_node*)file->fs_specific.tmpfs_pointer;
    if(node->mmap == 0)
        return -EINVAL;
    *out_phys = node->mmap;
    *out_size = node->mmap_size;
    return 0;
}

void devfs_close(file_descriptor* file) {
    auto node = (devfs_node*)file->fs_specific.tmpfs_pointer;
    if(node->close == nullptr)
        return;
    node->close(file, node);
    return;
}

signed long devfs_ls(file_descriptor* file, char* out, std::size_t count) {
    bool state = devfs_lock.lock();
    dirent* dir = (dirent*)out;
    if(file->other.ls_pointer == nullptr) {
        devfs_lock.unlock(state);
        return 0;
    } else if(file->other.ls_pointer == (void*)1) {
        file->other.ls_pointer = (void*)head_devfs_node;
    }

    devfs_node* node = (devfs_node*)file->other.ls_pointer;

    if(count < sizeof(dirent) + 1 + klibc::strlen(node->path)) {
        devfs_lock.unlock(state);
        return 0;
    }

    dir->d_ino = node->id;
    dir->d_reclen = sizeof(dirent) + 1 + klibc::strlen(node->path + 1);
    dir->d_type = DT_CHR;
    dir->d_off = 0;
    klibc::memcpy(dir->d_name, node->path + 1, klibc::strlen(node->path + 1) + 1);

    file->other.ls_pointer = (void*)(node->next);

    devfs_lock.unlock(state);
    return dir->d_reclen;
}

std::int32_t devfs_open(filesystem* fs, void* file_desc, char* path, bool is_directory) {
    (void)fs;
    auto node = devfs_lookup(path);

    if(node == nullptr)
        return -EINVAL;

    file_descriptor* fd = (file_descriptor*)file_desc;
    if(node->is_root && is_directory) {
        fd->vnode.stat = devfs_stat;
        fd->vnode.ls = devfs_ls;
        fd->other.ls_pointer = (void*)1;
        fd->fs_specific.tmpfs_pointer = (std::uint64_t)(&root_devfs_node);
        return 0;
    }

    fd->vnode.stat = devfs_stat;
    fd->vnode.ioctl = devfs_ioctl;
    fd->vnode.poll = devfs_poll;
    fd->vnode.mmap = devfs_mmap;
    fd->vnode.read = devfs_read;
    fd->vnode.write = devfs_write;
    fd->vnode.close = devfs_close;
    fd->other.is_a_tty = node->is_a_tty;
    fd->fs_specific.tmpfs_pointer = (std::uint64_t)node;

    if(node->open) 
        node->open(fd, node);
        
    return 0;

}

std::int32_t devfs_readlink(filesystem* fs, char* path, char* buffer) {
    (void)fs;
    (void)path;
    (void)buffer;
    return -EINVAL;
}

std::int32_t devfs_create(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode) {
    (void)fs;
    (void)path;
    (void)type;
    (void)mode;
    return -ENOTSUP;
}

std::int32_t devfs_remove(filesystem* fs, char* path) {
    (void)fs;
    (void)path;
    return -ENOTSUP;
}

void init(vfs::node* new_node) {
    root_devfs_node.is_root = true;
    new_node->fs->create = devfs_create;
    new_node->fs->open = devfs_open;
    new_node->fs->readlink = devfs_readlink;
    new_node->fs->remove = devfs_remove;
    klibc::memcpy(new_node->path, "/dev\0", sizeof("/dev\0") + 1);
}