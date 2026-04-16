#include <generic/vfs.hpp>
#include <generic/sysfs.hpp>
#include <cstdint>

#include <cstdint>
#include <generic/tmpfs.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <generic/vfs.hpp>
#include <klibc/string.hpp>
#include <utils/errno.hpp>
#include <utils/assert.hpp>
#include <utils/kernel_info.hpp>
#include <atomic>

tmpfs::tmpfs_node sysfs_root_node = {};
std::atomic<std::uint64_t> sysfs_id_ptr = 1;

bool sysfs_find_child(tmpfs::tmpfs_node* node, char* name, tmpfs::tmpfs_node** out) {
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

tmpfs::tmpfs_node* sysfs_lookup(char* path) {
    if(klibc::strcmp(path,"/\0") == 0)
        return &sysfs_root_node;
    
    tmpfs::tmpfs_node* current_node = &sysfs_root_node;

    char path_copy[4096];
    klibc::memcpy(path_copy, path, klibc::strlen(path) + 1);

    char* saveptr;
    char* token = klibc::strtok(&saveptr, path_copy, "/");

    while (token != nullptr) {
        bool status = sysfs_find_child(current_node, token, &current_node);
        if(status == false)
            return nullptr;
        token = klibc::strtok(&saveptr, nullptr, "/");
    }
    return current_node;
}

tmpfs::tmpfs_node* sysfs_get_parent(char* path) {
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
        return &sysfs_root_node;

    if (last_slash == path_copy) 
        return &sysfs_root_node;

    *last_slash = '\0';
    return sysfs_lookup(path_copy);
}

char* sysfs_get_name_from_path(char* path) {
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

std::int32_t sysfs_readlink(filesystem* fs, char* path, char* buffer) {
    fs->lock.lock();
    tmpfs::tmpfs_node* node = sysfs_lookup(path);
    if(node == nullptr) { fs->lock.unlock();
        return -ENOENT; }
 
    if(node->type != vfs_file_type::symlink) { fs->lock.unlock();
        return -EINVAL; }

    assert(node->content,"meeeow meeeeeow :3");

    klibc::memcpy(buffer, node->content, 4096);

    fs->lock.unlock();
    return 0;
}

signed long sysfs_ls(file_descriptor* file, char* out, std::size_t count) {
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
        auto current_node = node->directory_content[file->offset / sizeof(tmpfs::tmpfs_node**)];

        file->offset += sizeof(tmpfs::tmpfs_node**);

        if(current_node == nullptr)
            goto again;

        if(sizeof(dirent) + klibc::strlen(current_node->name) + 1 > count - current_offset) {
            file->vnode.fs->lock.unlock();
            klibc::debug_printf("%lli %lli\n", sizeof(dirent) + klibc::strlen(current_node->name) + 1, count - current_offset);
            return current_offset; 
        }

        dirent* current_dir = (dirent*)(out + current_offset);
        current_dir->d_ino = current_node->ino;
        current_dir->d_type = vfs_to_dt_type(current_node->type);
        current_dir->d_reclen = sizeof(dirent) + klibc::strlen(current_node->name) + 1;
        current_dir->d_off = 0;
        current_offset += current_dir->d_reclen;

        klibc::memcpy(current_dir->d_name, current_node->name, klibc::strlen(current_node->name) + 1);

    }

    file->vnode.fs->lock.unlock();
    return current_offset;
}

std::int32_t sysfs_create(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode) {
    (void)fs;
    (void)path;
    (void)type;
    (void)mode;
    return -ENOTSUP;
}

std::int32_t sysfs_internal_create(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode) {
    fs->lock.lock();
    tmpfs::tmpfs_node* parent = sysfs_get_parent(path);

    if(parent == nullptr) { fs->lock.unlock();
        return -ENOENT; }

    parent->size += sizeof(tmpfs::tmpfs_node*);

    if(parent->physical_size < parent->size) {
        alloc_t res = pmm::buddy::alloc(parent->size * sizeof(tmpfs::tmpfs_node*));
        tmpfs::tmpfs_node** new_dir = (tmpfs::tmpfs_node**)(res.phys + etc::hhdm());
        if(parent->directory_content) {
            klibc::memcpy(new_dir, parent->directory_content, parent->size);
            pmm::buddy::free((std::uint64_t)parent->directory_content - etc::hhdm());
        }
        parent->directory_content = new_dir;
        parent->physical_size = res.real_size;
    }

    tmpfs::tmpfs_node* new_node = (tmpfs::tmpfs_node*)(pmm::freelist::alloc_4k() + etc::hhdm());

    new_node->type = type;
    new_node->ino = sysfs_id_ptr++;
    new_node->mode = mode;
    klibc::memcpy(new_node->name, sysfs_get_name_from_path(path), klibc::strlen(sysfs_get_name_from_path(path)) + 1);

    parent->directory_content[(parent->size / sizeof(tmpfs::tmpfs_node**)) - 1] = new_node;

    fs->lock.unlock();
    return 0;
}

signed long sysfs_read(file_descriptor* file, void* buffer, std::size_t count) {
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

signed long sysfs_write(file_descriptor* file, void* buffer, std::size_t count) {
    assert(file->vnode.fs, "GSJGK");
    file->vnode.fs->lock.lock();
    tmpfs::tmpfs_node* node = (tmpfs::tmpfs_node*)(file->fs_specific.tmpfs_pointer);

    assert(node, "wtf!");

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

std::int32_t sysfs_stat(file_descriptor* file, stat* out) {
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

std::int32_t sysfs_open(filesystem* fs, void* file_desc, char* path, bool is_directory) {
    fs->lock.lock();
    tmpfs::tmpfs_node* node = sysfs_lookup(path);
    if(node == nullptr) { fs->lock.unlock();
        return -ENOENT; }

    if(is_directory && node->type != vfs_file_type::directory) { fs->lock.unlock();
        return -ENOTDIR; }
    
    file_descriptor* fd = (file_descriptor*)file_desc;
        
    fd->vnode.fs = fs;
    fd->vnode.stat = sysfs_stat;
    fd->vnode.read = sysfs_read;
    fd->vnode.ls = sysfs_ls;
    fd->fs_specific.tmpfs_pointer = (std::uint64_t)node;

    fs->lock.unlock();
    return 0;
}

void sysfs_dump_node(tmpfs::tmpfs_node* node, int depth, bool is_last) {

    for(int i = 0; i < depth; i++) {
        if(i == depth - 1 && !is_last)
            klibc::printf("├── ");
        else if(i == depth - 1 && is_last)
            klibc::printf("└── ");
        else
            klibc::printf("│   ");
    }
    
    klibc::printf("%s%s ", "/" , klibc::strcmp(node->name, "/") == 0 ? "sys" : node->name);
    
    switch(node->type) {
        case vfs_file_type::directory:
            klibc::printf("(directory)");
            break;
            
        case vfs_file_type::file:
            klibc::printf("(file) ");
            if(node->content && node->size > 0) {
                klibc::printf("content: \"");
                for(std::size_t i = 0; i < node->size && i < 50; i++) {
                    if((node->content[i] >= 32 && node->content[i] <= 126) || node->content[i] == 0)
                        klibc::printf("%c", node->content[i]);
                    else
                        klibc::printf(".");
                }
                if(node->size > 50)
                    klibc::printf("...");
                klibc::printf("\"");
            } else if(node->content) {
                klibc::printf("content: (empty)");
            } else {
                klibc::printf("content: (null)");
            }
            break;
            
        case vfs_file_type::symlink:
            klibc::printf("(symlink) ");
            if(node->content && node->size > 0) {
                klibc::printf("-> ");
                for(std::size_t i = 0; i < node->size; i++) {
                    klibc::printf("%c", node->content[i]);
                }
            } else {
                klibc::printf("-> (broken)");
            }
            break;
            
        default:
            klibc::printf("(unknown type: %d)", (int)node->type);
            break;
    }
    
    klibc::printf("\n");
    
    if(node->type == vfs_file_type::directory && node->directory_content) {
        std::uint64_t entry_count = node->size / sizeof(tmpfs::tmpfs_node*);
        
        for(std::uint64_t i = 0; i < entry_count; i++) {
            if(node->directory_content[i] != nullptr) {
                bool last_child = (i == entry_count - 1);
                for(std::uint64_t j = i + 1; j < entry_count; j++) {
                    if(node->directory_content[j] != nullptr) {
                        last_child = false;
                        break;
                    }
                    last_child = true;
                }
                sysfs_dump_node(node->directory_content[i], depth + 1, last_child);
            }
        }
    }
}

void sysfs::dump() {
    log("sysfs", "sysfs dump");
    klibc::printf("\n=== sysfs tree dump ===\n");
    sysfs_dump_node(&sysfs_root_node, 0, true);
    klibc::printf("========================\n\n");
}

filesystem* sys_fs = nullptr;

void sysfs::create_dir(const char* path, ...) {

    char buffer[4096];
    klibc::memset(buffer, 0, 4096);

    va_list val;
    va_start(val, path);
    int ret = klibc::_snprintf(buffer, 4096, path, val);
    va_end(val);

    (void)ret;

    int status2 = sysfs_internal_create(sys_fs, buffer, vfs_file_type::directory, 0666);
    if(status2 == -EEXIST)
        return;

    assert(status2 == 0, ":( %d", status2);
}

void sysfs::create(const char* path, void* content, std::size_t size, ...) {
    char buffer[4096];
    klibc::memset(buffer, 0, 4096);

    va_list val;
    va_start(val, size);
    int ret = klibc::_snprintf(buffer, 4096, path, val);
    va_end(val);

    (void)ret;

    int status1 = sysfs_internal_create(sys_fs, buffer, vfs_file_type::file, 0666);
    assert(status1 == 0, "shit %d", status1);
    file_descriptor fd = {};
    int status = sysfs_open(sys_fs, &fd, buffer, false);
    assert(status == 0, "muuuu %d", status);
    sysfs_write(&fd, content, size);
}

void sysfs::create_symlink(const char* path, char* target_path, ...) {

    char buffer[4096];
    klibc::memset(buffer, 0, 4096);

    va_list val;
    va_start(val, target_path);
    int ret = klibc::_snprintf(buffer, 4096, path, val);
    va_end(val);

    (void)ret;

    sysfs_internal_create(sys_fs, buffer, vfs_file_type::symlink, 0666);
    file_descriptor fd = {};
    sysfs_open(sys_fs, &fd, buffer, false);
    sysfs_write(&fd, target_path, klibc::strlen(target_path) + 1);
}

void sysfs::init(vfs::node* node) {
    sys_fs = new filesystem;
    node->fs = sys_fs;
    node->fs->open = sysfs_open;
    node->fs->create = sysfs_create;
    node->fs->readlink = sysfs_readlink;
    klibc::memcpy(node->path, "/sys/\0\0", sizeof("/sys/\0\0") + 1);

    alloc_t root_alloc = pmm::buddy::alloc(PAGE_SIZE);

    sysfs_root_node.size = 0;
    sysfs_root_node.type = vfs_file_type::directory;
    sysfs_root_node.physical_size = root_alloc.real_size;
    klibc::memcpy(sysfs_root_node.name, "/\0", sizeof("/\0") + 1);
    klibc::memcpy(node->internal_path, "/sys", sizeof("/sys\0") + 1);

    sysfs_root_node.directory_content = (tmpfs::tmpfs_node**)(root_alloc.phys + etc::hhdm());

    create_dir("/devices");
    create_dir("/bus");
    create_dir("/class");
    create_dir("/kernel");
    create_dir("/power");
    create_dir("/firmware");
    create("/kernel/version", (void*)kernel_info::version(), klibc::strlen(kernel_info::version()) + 1);

    create_dir("/class/graphics");

    file_descriptor test_fd = {};
    int status = vfs::open(&test_fd, (char*)"/sys/kernel/version", true, false);
    assert(status == 0, "meow :( %d",status);
    char buffer[256];
    klibc::memset(buffer,0,256);
    status = test_fd.vnode.read(&test_fd, buffer, 256);

    log("sysfs", "/sys/kernel/version content is %s (status %d)", buffer, status);

}