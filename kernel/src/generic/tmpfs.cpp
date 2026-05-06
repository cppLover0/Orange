#include <cstdint>
#include <generic/tmpfs.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <generic/vfs.hpp>
#include <klibc/string.hpp>
#include <utils/errno.hpp>
#include <utils/assert.hpp>
#include <atomic>
#include <generic/time.hpp>

tmpfs::tmpfs_node root_node = {};
std::atomic<std::uint64_t> tmpfs_id_ptr = 1;

bool tmpfs_find_child(tmpfs::tmpfs_node* node, char* name, tmpfs::tmpfs_node** out) {
    if(node->type != vfs_file_type::directory)
        return false;

    for(std::uint64_t i = 0;i < node->size / sizeof(tmpfs::directory_cont);i++ ) {

        if(node->dirents[i].node == nullptr)
            continue;

        if(klibc::strcmp(node->dirents[i].name, name) == 0) {
            *out = node->dirents[i].node;
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

void tmpfs_get_name_parent(char* path, char* out) {
    char path_copy[4096];
    klibc::memcpy(path_copy, path, klibc::strlen(path) + 1);

    char* last_slash = nullptr;
    for (int i = 0; path_copy[i] != '\0'; i++) {
        if (path_copy[i] == '/') {
            last_slash = &path_copy[i];
        }
    }

    if (last_slash == nullptr) {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    if (last_slash == path_copy) {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    *last_slash = '\0';

    klibc::memcpy(out, path_copy, klibc::strlen(path_copy) + 1);
    return;
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

    if(!node->content) {
        fs->lock.unlock();
        return -EINVAL;
    }

    assert(node->content,"meeeow meeeeeow :3 %lli", node->ino);

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
        //klibc::debug_printf("%lli %lli\n", file->offset, node->size);
        file->vnode.fs->lock.unlock();
        return current_offset; 
    }

    while(true) {

        tmpfs::directory_cont* dre = &node->dirents[file->offset / sizeof(tmpfs::directory_cont)];

        auto current_node = dre->node;

        file->offset += sizeof(tmpfs::directory_cont);

        if(file->offset + sizeof(tmpfs::directory_cont) >= node->size) {
            //klibc::debug_printf("%lli %lli\n", file->offset, node->size);
            file->vnode.fs->lock.unlock();
            return current_offset; 
        }

        if(current_node == nullptr)
            goto again;

        if(sizeof(dirent) + klibc::strlen(dre->name) + 1 > count - current_offset) {
            file->vnode.fs->lock.unlock();
            return current_offset; 
        }

        dirent* current_dir = (dirent*)(out + current_offset);
        current_dir->d_ino = current_node->ino;
        current_dir->d_type = vfs_to_dt_type(current_node->type);
        current_dir->d_reclen = sizeof(dirent) + klibc::strlen(dre->name) + 1;
        current_dir->d_off = 0;
        current_offset += current_dir->d_reclen;

        klibc::memcpy(current_dir->d_name, dre->name, klibc::strlen(dre->name) + 1);

    }

    file->vnode.fs->lock.unlock();
    return current_offset;
}

std::int32_t tmpfs_link(filesystem* fs, char* old_path, char* new_path) {
    fs->lock.lock();

    tmpfs::tmpfs_node* old_node = tmpfs_lookup(old_path);
    if(old_node == nullptr) { fs->lock.unlock();
        return -ENOENT; }

    tmpfs::tmpfs_node* parent = tmpfs_get_parent(new_path);
    if(parent == nullptr) {
        fs->lock.unlock();
        return -ENOENT;
    }

    if(old_node->type == vfs_file_type::directory) {
        fs->lock.unlock();
        return -EPERM;
    }

    bool is_pasted = false;

    again_paste:
    for(std::uint64_t i = 0;i < parent->size / sizeof(tmpfs::directory_cont); i++) {
        if(parent->dirents[i].node == nullptr)  {
            parent->dirents[i].node = old_node;
            klibc::memcpy(parent->dirents[i].name, tmpfs_get_name_from_path(new_path), klibc::strlen(tmpfs_get_name_from_path(new_path)) + 1);
            is_pasted = true;
            break;
        }
    }

    if(is_pasted == false) {
        alloc_t res = pmm::buddy::alloc(parent->size * sizeof(tmpfs::directory_cont));
        tmpfs::directory_cont* new_dir = (tmpfs::directory_cont*)(res.phys + etc::hhdm());
        if(parent->dirents) {
            klibc::memcpy(new_dir, parent->dirents, parent->size);
            pmm::buddy::free((std::uint64_t)parent->dirents - etc::hhdm());
        }
        parent->dirents = new_dir;
        parent->physical_size = res.real_size;
        parent->size = parent->physical_size;
        goto again_paste;
    }

    old_node->nlink++;
    
    fs->lock.unlock();
    return 0;
}

std::uint8_t __tmpfs__create_parent_dirs_by_default = 1; /* Used for ustar */

std::int32_t tmpfs_create(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode, tmpfs::tmpfs_node** out = nullptr) {

    if(fs)
        fs->lock.lock();

    tmpfs::tmpfs_node* parent = tmpfs_get_parent(path);

    if(!parent) {
        char copy[4096];
        tmpfs_get_name_parent(path, copy);
        if(!__tmpfs__create_parent_dirs_by_default) {
            if(fs)
                fs->lock.unlock();
            return -ENOENT;
        } else {
            tmpfs_create(nullptr, copy, vfs_file_type::directory, mode, &parent);
        }
    }

    if(parent == nullptr) { 
        if(fs)
            fs->lock.unlock();
        return -ENOENT; }

    bool is_pasted = false;

    tmpfs::tmpfs_node* new_node = (tmpfs::tmpfs_node*)(pmm::freelist::alloc_4k() + etc::hhdm());

again_paste:
    for(std::uint64_t i = 0;i < parent->size / sizeof(tmpfs::directory_cont); i++) {
        if(parent->dirents[i].node == nullptr)  {
            parent->dirents[i].node = new_node;
            klibc::memcpy(parent->dirents[i].name, tmpfs_get_name_from_path(path), klibc::strlen(tmpfs_get_name_from_path(path)) + 1);
            is_pasted = true;
            break;
        }
    }

    if(is_pasted == false) {
        alloc_t res = pmm::buddy::alloc(parent->size * sizeof(tmpfs::directory_cont));
        tmpfs::directory_cont* new_dir = (tmpfs::directory_cont*)(res.phys + etc::hhdm());
        if(parent->dirents) {
            klibc::memcpy(new_dir, parent->dirents, parent->size);
            pmm::buddy::free((std::uint64_t)parent->dirents - etc::hhdm());
        }
        parent->dirents = new_dir;
        parent->physical_size = res.real_size;
        parent->size = parent->physical_size;
        goto again_paste;
    }

    new_node->type = type;
    new_node->ino = tmpfs_id_ptr++;
    new_node->mode = mode;
    new_node->create_time = time::current_unix_time;
    new_node->nlink = 1;

    if(!fs) {
        if(out)
            *out = new_node;
    }

    if(fs)
        fs->lock.unlock();
    return 0;
}

std::int32_t tmpfs_opt_create(char* path, vfs_file_type type, std::uint32_t mode, char* content, std::uint64_t size, tmpfs::tmpfs_node** out = nullptr) {
    (void)out;
    tmpfs::tmpfs_node* parent = tmpfs_get_parent(path);

    if(!parent) {
        char copy[4096];
        tmpfs_get_name_parent(path, copy);
        if(!__tmpfs__create_parent_dirs_by_default) {
            return -ENOENT;
        } else {
            tmpfs_create(nullptr, copy, vfs_file_type::directory, mode, &parent);
        }
    }

    if(parent == nullptr) { 
        return -ENOENT; }

    bool is_pasted = false;

    tmpfs::tmpfs_node* new_node = (tmpfs::tmpfs_node*)(pmm::freelist::alloc_4k() + etc::hhdm());

again_paste:
    for(std::uint64_t i = 0;i < parent->size / sizeof(tmpfs::directory_cont); i++) {
        if(parent->dirents[i].node == nullptr)  {
            parent->dirents[i].node = new_node;
            klibc::memcpy(parent->dirents[i].name, tmpfs_get_name_from_path(path), klibc::strlen(tmpfs_get_name_from_path(path)) + 1);
            is_pasted = true;
            break;
        }
    }

    if(is_pasted == false) {
        alloc_t res = pmm::buddy::alloc(parent->size * sizeof(tmpfs::directory_cont));
        tmpfs::directory_cont* new_dir = (tmpfs::directory_cont*)(res.phys + etc::hhdm());
        if(parent->dirents) {
            klibc::memcpy(new_dir, parent->dirents, parent->size);
            pmm::buddy::free((std::uint64_t)parent->dirents - etc::hhdm());
        }
        parent->dirents = new_dir;
        parent->physical_size = res.real_size;
        parent->size = parent->physical_size;
        goto again_paste;
    }

    new_node->type = type;
    new_node->ino = tmpfs_id_ptr++;
    new_node->mode = mode;
    new_node->create_time = time::current_unix_time;
    new_node->nlink = 1;

    if(new_node->type != vfs_file_type::directory) {
        alloc_t new_content = pmm::buddy::alloc(size);
        char* new_cont = (char*)(new_content.phys + etc::hhdm());
        new_node->physical_size = new_content.real_size;
        new_node->size = size;
        new_node->content = new_cont;
        klibc::memcpy(new_cont, content, size);
    }

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
    node->modify_time = time::current_unix_time;
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

std::int32_t tmpfs_chmod(file_descriptor* fd, int chmod) {
    fd->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)(fd->fs_specific.tmpfs_pointer);
    node->mode = chmod;
    fd->vnode.fs->lock.unlock();
    return 0;
}

bool tmpfs_test_for_busy(tmpfs::tmpfs_node* node) {
    if(node->busy_counter > 0)
        return true;

    if(node->type == vfs_file_type::directory) {
        for(std::uint64_t i = 0;i < node->size / sizeof(tmpfs::directory_cont);i++ ) {
            klibc::debug_printf("grrrrr");
            assert(node->dirents, "werr %d", node->ino);
            if(node->dirents[i].node == nullptr)
                continue;

            if(tmpfs_test_for_busy(node->dirents[i].node) == true)
                return true;
        }
    }
    return false;
}

std::int32_t tmpfs_internal_remove(tmpfs::tmpfs_node* node) {

    klibc::debug_printf("rm 0x%p", node);

    if(tmpfs_test_for_busy(node) || node->nlink != 0)
        return -EBUSY;

    if(node->content) {
        pmm::buddy::free((std::uint64_t)node->content);
        node->content = 0;
    }

    pmm::freelist::free((std::uint64_t)node - etc::hhdm());
    return 0;
}

std::int32_t tmpfs_remove(filesystem* fs, char* path) {
    (void)fs;
    (void)path;
    assert(0, "h");
    return 0;
}

std::int32_t tmpfs_zero(file_descriptor* file) {
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)(file->fs_specific.tmpfs_pointer);
    if(node->content) 
        pmm::buddy::free((std::uint64_t)node->content - etc::hhdm());
    node->size = 0;
    node->physical_size = 0;
    node->content = 0;
    file->vnode.fs->lock.unlock();
    return 0;
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
    (void)type_to_mode;
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)file->fs_specific.tmpfs_pointer;
    out->st_gid = 0;
    out->st_uid = 0;
    out->st_rdev = 0;
    out->st_blksize = PAGE_SIZE;
    out->st_blocks = node->size / 512;
    out->st_mode = type_to_mode(node->type) | node->mode;
    out->st_size = node->size;
    out->st_ino = node->ino;
    out->st_nlink = node->nlink;
    out->st_atim.tv_sec = node->access_time;
    out->st_mtim.tv_sec = node->modify_time;
    out->st_ctim.tv_sec = node->create_time;
    node->access_time = time::current_unix_time;

    assert(node->busy_counter >= 0, "Fx");

    file->vnode.fs->lock.unlock();
    return 0;
}

void tmpfs_close(file_descriptor* file) {
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)file->fs_specific.tmpfs_pointer;
    node->busy_counter--;

    assert(node->busy_counter >= 0, "Fb");

    if(node->busy_counter == 0 && node->nlink == 0) { 
        klibc::debug_printf("rm");
        int res = tmpfs_internal_remove(node);
        if(res != 0) {
            klibc::debug_printf("rm unlink res %d path %s\n",res, file->path);
        }
    }

    file->vnode.fs->lock.unlock();
}

void tmpfs_ondup(file_descriptor* file) {
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)file->fs_specific.tmpfs_pointer;
    node->busy_counter++;
    file->vnode.fs->lock.unlock();
    return;
}

std::int32_t tmpfs_open(filesystem* fs, void* file_desc, char* path, bool is_directory) {
    fs->lock.lock();
    tmpfs::tmpfs_node* node = tmpfs_lookup(path);
    if(node == nullptr) { fs->lock.unlock();
        return -ENOENT; }

    node->access_time = time::current_unix_time;

    if(is_directory && node->type != vfs_file_type::directory) { fs->lock.unlock();
        return -ENOTDIR; }
    
    file_descriptor* fd = (file_descriptor*)file_desc;

    assert(node->busy_counter >= 0, "F");
        
    fd->vnode.fs = fs;
    fd->vnode.stat = tmpfs_stat;
    fd->vnode.read = tmpfs_read;
    fd->vnode.write = tmpfs_write;
    fd->vnode.ls = tmpfs_ls;
    fd->vnode.zero = tmpfs_zero;
    fd->vnode.chmod = tmpfs_chmod;
    fd->vnode.close = tmpfs_close;
    fd->vnode.ondup = tmpfs_ondup;
    fd->fs_specific.tmpfs_pointer = (std::uint64_t)node;

    node->busy_counter++;

    fs->lock.unlock();
    return 0;
}

std::int32_t tmpfs_unlink(filesystem* fs, char* path) {
    fs->lock.lock();
    tmpfs::tmpfs_node* node = tmpfs_lookup(path);
    if(node == nullptr) { fs->lock.unlock();
        return -ENOENT; }

    if(node->type == vfs_file_type::directory) { fs->lock.unlock();
        return -EISDIR;
    }

    char name[257] = {};
    klibc::memcpy(name, tmpfs_get_name_from_path(path), klibc::strlen(tmpfs_get_name_from_path(path)) + 1);

    tmpfs::tmpfs_node* parent = tmpfs_get_parent(path);

    for(std::uint64_t i = 0;i < parent->size / sizeof(tmpfs::directory_cont); i++) {
        if(klibc::strcmp(name, parent->dirents[i].name) == 0) {
            parent->dirents[i].node = nullptr;
            klibc::memset(parent->dirents[i].name, 0, sizeof(parent->dirents[i].name));
        }
    }

    if(parent == nullptr) { 
        if(fs)
            fs->lock.unlock();
        assert(0, "weird things happen...");
        return -ENOENT; }

    node->nlink--;

    assert(node->busy_counter >= (std::int64_t)0, "v");

    if(node->busy_counter == 0 && node->nlink == 0) {
        int res = tmpfs_internal_remove(node);
        if(res != 0) {
            klibc::debug_printf("rm unlink res %d path %s\n",res, path);
        }
    }

    fs->lock.unlock();
    return 0;
}

void tmpfs::init_default(vfs::node* node) {
    filesystem* new_fs = new filesystem;
    node->fs = new_fs;
    node->fs->open = tmpfs_open;
    node->fs->create = (int (*)(filesystem *, char *, vfs_file_type, unsigned int))((std::uint64_t)tmpfs_create);
    node->fs->readlink = tmpfs_readlink;
    node->fs->remove = tmpfs_remove;
    node->fs->link = tmpfs_link;
    node->fs->unlink = tmpfs_unlink;
    klibc::memcpy(node->path, "/\0\0", sizeof("/\0\0") + 1);

    alloc_t root_alloc = pmm::buddy::alloc(PAGE_SIZE);

    root_node.size = 0;
    root_node.type = vfs_file_type::directory;
    root_node.physical_size = root_alloc.real_size;
    root_node.nlink = 9999999;
    root_node.size = root_node.physical_size;
    klibc::memcpy(node->internal_path, "/", sizeof("/\0") + 1);

    root_node.dirents = (tmpfs::directory_cont*)(root_alloc.phys + etc::hhdm());
    log("tmpfs", "root_node is 0x%p",&root_node);

}