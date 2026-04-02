#include <cstdint>
#include <generic/tmpfs.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <generic/vfs.hpp>
#include <klibc/string.hpp>
#include <utils/errno.hpp>
#include <utils/assert.hpp>
#include <atomic>

tmpfs::tmpfs_node root_node = {};
std::atomic<std::uint64_t> tmpfs_id_ptr = 1;

bool tmpfs_find_child(tmpfs::tmpfs_node* node, char* name, tmpfs::tmpfs_node** out) {
    if(node->type != vfs_file_type::directory)
        return false;

    for(std::uint64_t i = 0;i < node->size / sizeof(std::uint64_t);i++ ) {

        if(node->directory_content[i] == nullptr)
            continue;

        if(klibc::strcmp(node->directory_content[i]->name, name) == 0) {
            *out = node->directory_content[i];
            return true;
        }
    }

    return false;
}

tmpfs::tmpfs_node* tmpfs_lookup(char* path) {
    if(klibc::strcmp(path,"/\0") == 0)
        return &root_node;
    
    tmpfs::tmpfs_node* current_node = &root_node;

    char path_copy[4096];
    klibc::memcpy(path_copy, path, klibc::strlen(path) + 1);

    char* saveptr;
    char* token = klibc::strtok(&saveptr, path_copy, "/");

    while (token != nullptr) {
        bool status = tmpfs_find_child(current_node, token, &current_node);
        if(status == false)
            return nullptr;
        token = klibc::strtok(&saveptr, nullptr, "/");
    }
    return current_node;
}

tmpfs::tmpfs_node* tmpfs_get_parent(char* path) {
    if (klibc::strcmp(path, "/") == 0 || klibc::strlen(path) == 0) {
        return nullptr;
    }

    char path_copy[4096];
    klibc::memcpy(path_copy, path, klibc::strlen(path) + 1);

    char* last_slash = nullptr;
    for (int i = 0; path_copy[i] != '\0'; i++) {
        if (path_copy[i] == '/') {
            last_slash = &path_copy[i];
        }
    }

    if (last_slash == nullptr) 
        return &root_node;

    if (last_slash == path_copy) 
        return &root_node;

    *last_slash = '\0';
    return tmpfs_lookup(path_copy);
}

char* tmpfs_get_name_from_path(char* path) {
    if (path[0] == '\0') return path;
    if (klibc::strcmp(path, "/") == 0) return path;

    char* last_slash = nullptr;
    int i = 0;

    while (path[i] != '\0') {
        if (path[i] == '/') {
            last_slash = &path[i];
        }
        i++;
    }

    if (last_slash == nullptr) 
        return path;

    return last_slash + 1;
}

std::int32_t tmpfs_readlink(filesystem* fs, char* path, char* buffer) {
    fs->lock.lock();
    tmpfs::tmpfs_node* node = tmpfs_lookup(path);
    if(node == nullptr) { fs->lock.unlock();
        return -ENOENT; }
 
    if(node->type != vfs_file_type::symlink) { fs->lock.unlock();
        return -EINVAL; }

    assert(node->content,"meeeow meeeeeow :3");

    klibc::memcpy(buffer, node->content, 4096);

    fs->lock.unlock();
    return 0;
}

signed long tmpfs_ls(file_descriptor* file, char* out, std::size_t count) {
    file->vnode.fs->lock.lock();
    auto node = (tmpfs::tmpfs_node*)file->fs_specific.tmpfs_pointer;

    std::size_t current_offset = 0;

    if(node->type != vfs_file_type::directory)
        return -ENOTDIR;

again:

    if(file->offset >= node->size) {
        file->vnode.fs->lock.unlock();
        return 0; 
    }

    while(true) {
        auto current_node = node->directory_content[file->offset];
        if(sizeof(dirent) + klibc::strlen(current_node->name) + 1 > count - current_offset) {
            file->vnode.fs->lock.unlock();
            return current_offset; 
        }

        file->offset += sizeof(tmpfs::tmpfs_node**);

        if(current_node == nullptr)
            goto again;

        dirent* current_dir = (dirent*)(out + current_offset);
        current_dir->d_ino = current_node->ino;
        current_dir->d_type = vfs_to_dt_type(current_node->type);
        current_dir->d_reclen = sizeof(dirent) + klibc::strlen(current_node->name) + 1;
        current_dir->d_off = 0;
        current_offset += current_dir->d_reclen;

    }

    file->vnode.fs->lock.unlock();
    return current_offset;
}

std::int32_t tmpfs_create(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode) {
    fs->lock.lock();
    tmpfs::tmpfs_node* parent = tmpfs_get_parent(path);

    if(parent == nullptr) { fs->lock.unlock();
        return -ENOENT; }

    parent->size += sizeof(tmpfs::tmpfs_node*);

    if(parent->physical_size < parent->size) {
        alloc_t res = pmm::buddy::alloc(parent->size * sizeof(tmpfs::tmpfs_node*));
        tmpfs::tmpfs_node** new_dir = (tmpfs::tmpfs_node**)(res.phys + etc::hhdm());
        klibc::memcpy(new_dir, parent->directory_content, parent->size);
        pmm::buddy::free((std::uint64_t)parent->directory_content - etc::hhdm());
        parent->directory_content = new_dir;
        parent->physical_size = res.real_size;
    }

    tmpfs::tmpfs_node* new_node = (tmpfs::tmpfs_node*)(pmm::freelist::alloc_4k() + etc::hhdm());

    new_node->type = type;
    new_node->ino = tmpfs_id_ptr++;
    new_node->mode = mode;
    klibc::memcpy(new_node->name, tmpfs_get_name_from_path(path), klibc::strlen(tmpfs_get_name_from_path(path)) + 1);

    parent->directory_content[(parent->size / sizeof(tmpfs::tmpfs_node**)) - 1] = new_node;

    fs->lock.unlock();
    return 0;
}

signed long tmpfs_read(file_descriptor* file, void* buffer, std::size_t count) {
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)(file->fs_specific.tmpfs_pointer);
    if(node->type == vfs_file_type::directory) { file->vnode.fs->lock.unlock();
        return -EISDIR; }
    
    if(file->offset >= node->size || node->content == nullptr) {
        file->vnode.fs->lock.unlock();
        return 0;
    }

    std::size_t available = node->size - file->offset;
    std::size_t to_read = (count > available) ? available : count;
    klibc::memcpy(buffer, node->content + file->offset, to_read);
    file->offset += to_read;

    file->vnode.fs->lock.unlock();
    return to_read;
}

signed long tmpfs_write(file_descriptor* file, void* buffer, std::size_t count) {
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)(file->fs_specific.tmpfs_pointer);
    if(node->type == vfs_file_type::directory) { file->vnode.fs->lock.unlock();
        return -EISDIR; }
    
    if(file->offset + count > node->physical_size || node->content == nullptr) {
        alloc_t new_content = pmm::buddy::alloc(file->offset + count);
        char* new_cont = (char*)(new_content.phys + etc::hhdm());
        if(node->content) {
            klibc::memcpy(new_cont, node->content, node->size);
            pmm::buddy::free((std::uint64_t)node->content - etc::hhdm());
        }
        node->content = new_cont;
        node->physical_size = new_content.real_size;
    }

    if(file->offset + count > node->size)
        node->size = file->offset + count;

    klibc::memcpy(node->content + file->offset, buffer, count);

    file->offset += count;
    file->vnode.fs->lock.unlock();
    return count;
}

inline static std::int32_t type_to_mode(vfs_file_type type) {
    switch (type)
    {   

    case vfs_file_type::directory:
        return S_IFDIR;
    
    case vfs_file_type::file:
        return S_IFREG;
    
    case vfs_file_type::symlink:
        return S_IFLNK;

    default:
        assert(0,"say gex");
    }
}

std::int32_t tmpfs_stat(file_descriptor* file, stat* out) {
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)file->fs_specific.tmpfs_pointer;
    out->st_gid = 0;
    out->st_uid = 0;
    out->st_rdev = 0;
    out->st_blksize = PAGE_SIZE;
    out->st_blocks = node->physical_size / PAGE_SIZE;
    out->st_mode = type_to_mode(node->type) | node->mode;
    out->st_size = node->size;
    out->st_ino = node->ino;
    file->vnode.fs->lock.unlock();
    return 0;
}

std::int32_t tmpfs_open(filesystem* fs, void* file_desc, char* path, bool is_directory) {
    fs->lock.lock();
    tmpfs::tmpfs_node* node = tmpfs_lookup(path);
    if(node == nullptr) { fs->lock.unlock();
        return -ENOENT; }

    if(is_directory && node->type != vfs_file_type::directory) { fs->lock.unlock();
        return -ENOTDIR; }
    
    file_descriptor* fd = (file_descriptor*)file_desc;
        
    fd->vnode.fs = fs;
    fd->vnode.stat = tmpfs_stat;
    fd->vnode.read = tmpfs_read;
    fd->vnode.write = tmpfs_write;
    fd->vnode.ls = tmpfs_ls;
    fd->fs_specific.tmpfs_pointer = (std::uint64_t)node;

    fs->lock.unlock();
    return 0;
}

void tmpfs::init_default(vfs::node* node) {
    filesystem* new_fs = new filesystem;
    node->fs = new_fs;
    node->fs->open = tmpfs_open;
    node->fs->create = tmpfs_create;
    node->fs->readlink = tmpfs_readlink;
    klibc::memcpy(node->path, "/tmp/\0\0", sizeof("/tmp/\0\0") + 1);

    alloc_t root_alloc = pmm::buddy::alloc(PAGE_SIZE);

    root_node.size = 0;
    root_node.physical_size = root_alloc.real_size;
    klibc::memcpy(root_node.name, "/\0", sizeof("/\0") + 1);
    klibc::memcpy(node->internal_path, "/tmp", sizeof("/tmp\0") + 1);

    root_node.directory_content = (tmpfs::tmpfs_node**)(root_alloc.phys + etc::hhdm());
    log("tmpfs", "root_node is 0x%p",&root_node);

}