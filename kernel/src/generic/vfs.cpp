#include <cstdint>
#include <generic/vfs.hpp>
#include <drivers/disk.hpp>
#include <klibc/string.hpp>
#include <utils/errno.hpp>
#include <klibc/stdio.hpp>
#include <generic/lock/mutex.hpp>
#include <utils/assert.hpp>
#include <generic/tmpfs.hpp>
#include <generic/devfs.hpp>
#include <generic/evdev.hpp>
#include <generic/sysfs.hpp>

// /bin/path -> /usr/bin/path
void __vfs_symlink_resolve_no_at_symlink_follow(char* path, char* out) {
    char *next = nullptr;
    char buffer[4096];

    int e = vfs::readlink(path,buffer,4096);

    if(e == -ENOSYS) 
        klibc::memcpy(out,path,klibc::strlen(path) + 1);

    if(e == -EINVAL)
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    else if(e == 0) {
        
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    } else if(e == -ENOENT) {
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
        // maybe it wants directory symlink ?
        char path0[4096];
        klibc::memcpy(path0,path,klibc::strlen(path) + 1);

        char result[4096];

        int c = 0;

        char* token = klibc::strtok(&next, path0, "/");
        
        /* Start building path from null and trying to find symlink */
        while(token) {
            
            result[c] = '/';
            c++;

            std::uint64_t mm = 0;
            mm = klibc::strlen(token);
            klibc::memcpy((char*)((std::uint64_t)result + c),token,mm);
            c += mm;
            result[c] = '\0';

            e = vfs::readlink(result,buffer,4096);
            if(e == 0) {
                char buffer2[4096];
                vfs::resolve_path(buffer,result,buffer2,0);
                c = klibc::strlen(buffer2);
                buffer2[c++] = '/';
                klibc::memcpy(buffer2 + c, path + klibc::strlen(result) + 1, klibc::strlen(path) - klibc::strlen(result));
                __vfs_symlink_resolve_no_at_symlink_follow(buffer2,out);
                return;
            } else if(e == -ENOENT) {
                klibc::memcpy(out,path,klibc::strlen(path) + 1);
            }

            token = klibc::strtok(&next,0,"/");
        }
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    }
}

void __vfs_symlink_resolve(char* path, char* out, int level) {

    char *next = nullptr;

    char buffer[4096];

    if(level == 25) {
        // hell no :skull:
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
        return;
    }

    int e = vfs::readlink(path,buffer,4096);

    if(e == -ENOSYS) 
        klibc::memcpy(out,path,klibc::strlen(path) + 1);

    if(e == -EINVAL)
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    else if(e == 0) {
        
        char result[4096];
        vfs::resolve_path(buffer,path,result,0);
        __vfs_symlink_resolve(result,out, level + 1);
    } else if(e == -ENOENT) {
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
        // maybe it wants directory symlink ?
        char path0[4096];
        klibc::memcpy(path0,path,klibc::strlen(path) + 1);

        char result[4096];

        int c = 0;

        char* token = klibc::strtok(&next, path0, "/");
        
        /* Start building path from null and trying to find symlink */
        while(token) {
            
            result[c] = '/';
            c++;

            std::uint64_t mm = 0;
            mm = klibc::strlen(token);
           klibc:: memcpy((char*)((std::uint64_t)result + c),token,mm);
            c += mm;
            result[c] = '\0';

            e = vfs::readlink(result,buffer,4096);
            if(e == 0) {
                char buffer2[4096];
                vfs::resolve_path(buffer,result,buffer2,0);
                c = klibc::strlen(buffer2);
                buffer2[c++] = '/';
                klibc::memcpy(buffer2 + c, path + klibc::strlen(result) + 1, klibc::strlen(path) - klibc::strlen(result));
                __vfs_symlink_resolve(buffer2,out, level + 1);
                return;
            } else if(e == -ENOENT) {
                klibc::memcpy(out,path,klibc::strlen(path) + 1);
                return;
            } else {
                klibc::memcpy(out,path,klibc::strlen(path) + 1);
                return;
            }

            token = klibc::strtok(&next,0,"/");
        }
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    }
}

// vfs_nodes[0] is always root
vfs::node vfs_nodes[512];

vfs::node* find_node(char* path) {
    int r = 0;
    vfs::node* match;
    for(int i = 0;i < 512;i++) {

        if(klibc::strcmp(path, vfs_nodes[i].internal_path) == 0) 
            return &vfs_nodes[i];

        if(!klibc::strncmp(path,vfs_nodes[i].path,klibc::strlen(vfs_nodes[i].path))) {
            if(klibc::strlen(vfs_nodes[i].path) > r) {
                match = &vfs_nodes[i];
            }
        }

        if(!klibc::strncmp(path, vfs_nodes[i].path,klibc::strlen(vfs_nodes[i].path) - 1) && path[klibc::strlen(vfs_nodes[i].path) + 1] == '\0') {
            return &vfs_nodes[i];
        }

    }
    return match;
}

std::int32_t vfs::open(file_descriptor* fd, char* path, bool follow_symlinks, bool is_directory) {
    char out[4096];
    klibc::memset(out,0,4096);

    if(follow_symlinks) {
        __vfs_symlink_resolve(path, out, 0);
    } else {
        __vfs_symlink_resolve_no_at_symlink_follow(path, out);
    }

    node* node = find_node(out);

    if(!node) { 
        return -ENOENT; }
    
    char* fs_love_name = out + klibc::strlen(node->path) - 1;
    if(!node->fs->open) { assert(0, "meow :3");
        return -ENOSYS; }

    if(fs_love_name[0] == '\0') {
        fs_love_name[0] = '/';
        fs_love_name[1] = '\0';
    }

    std::int32_t status = node->fs->open(node->fs, (void*)fd, fs_love_name, is_directory);
    return status;
}

std::int32_t vfs::create(char* path, vfs_file_type type, std::uint32_t mode) {
    char out[4096];
    klibc::memset(out,0,4096);

    __vfs_symlink_resolve_no_at_symlink_follow(path, out);

    node* node = find_node(out);

    if(!node) { 
        return -ENOENT; }
    
    char* fs_love_name = out + klibc::strlen(node->path) - 1;
    if(!node->fs->create) { assert(0, "meow :3");
        return -ENOSYS; }

    if(fs_love_name[0] == '\0' || klibc::strcmp(path,node->internal_path) == 0) {
        fs_love_name[0] = '/';
        fs_love_name[1] = '\0';
    }

    std::int32_t status = node->fs->create(node->fs, fs_love_name, type, mode);
    return status;
}

std::int32_t vfs::readlink(char* path, char* out, std::uint32_t out_len) {

    if(path[0] != '/') {
        log("vfs", "unhealth shit %s", path);
        asm volatile("ud2");
    }

    assert(out_len == 4096, "stfu");

    char outp[4096];
    klibc::memset(outp,0,4096);

    klibc::memcpy(outp, path, klibc::strlen(path) + 1);

    node* node = find_node(outp);

    if(!node) { 
        return -ENOENT; }

    char* fs_love_name = outp + klibc::strlen(node->path) - 1;
    if(!node->fs->readlink) { assert(0, "meow :3");
        return -ENOSYS; }

    if(fs_love_name[0] == '\0' || klibc::strcmp(path,node->internal_path) == 0) {
        fs_love_name[0] = '/';
        fs_love_name[1] = '\0';
    }

    std::int32_t status = node->fs->readlink(node->fs, fs_love_name, out);
    return status;
}

std::int32_t vfs::remove(char* path) {
    char out[4096];
    klibc::memset(out,0,4096);

    __vfs_symlink_resolve_no_at_symlink_follow(path, out);

    node* node = find_node(out);

    if(!node) { 
        return -ENOENT; }
    
    char* fs_love_name = out + klibc::strlen(node->path) - 1;
    if(!node->fs->remove) { assert(0, "meow remove :3");
        return -ENOSYS; }

    if(fs_love_name[0] == '\0' || klibc::strcmp(path,node->internal_path) == 0) {
        fs_love_name[0] = '/';
        fs_love_name[1] = '\0';
    }

    std::int32_t status = node->fs->remove(node->fs, fs_love_name);
    return status;
}

std::int32_t vfs::unlink(char* path) {
    char out[4096];
    klibc::memset(out,0,4096);

    __vfs_symlink_resolve_no_at_symlink_follow(path, out);

    node* node = find_node(out);

    if(!node) { 
        return -ENOENT; }
    
    char* fs_love_name = out + klibc::strlen(node->path) - 1;
    if(!node->fs->unlink) { assert(0, "meow unlink :3");
        return -ENOSYS; }

    if(fs_love_name[0] == '\0' || klibc::strcmp(path,node->internal_path) == 0) {
        fs_love_name[0] = '/';
        fs_love_name[1] = '\0';
    }

    std::int32_t status = node->fs->unlink(node->fs, fs_love_name);
    return status;
}

// will init default environment

void vfs::init() {
    klibc::memset(vfs_nodes, 0, sizeof(vfs_nodes));
    tmpfs::init_default(&vfs_nodes[1]);
    evdev::init_default(&vfs_nodes[2]);
    devfs::init(&vfs_nodes[3]);
    sysfs::init(&vfs_nodes[4]);

    log("vfs", "test /dev/ptmx node is %s", find_node((char*)"/dev/ptmx")->path);
    log("vfs", "vfs_nodes 0x%p",vfs_nodes);
}