
#include <cstdint>
#include <generic/vfs/tmpfs.hpp>
#include <generic/vfs/vfs.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/heap.hpp>

#include <etc/etc.hpp>

#include <etc/libc.hpp>
#include <etc/errno.hpp>

#include <drivers/cmos.hpp>

#include <etc/logging.hpp>

vfs::tmpfs_node_t* head_node;

vfs::tmpfs_node_t* __tmpfs__find(char* path) {
    vfs::tmpfs_node_t* current = head_node;
    while(current) {
        if(current->type != TMPFS_TYPE_NONE && current->name[0] != '\0') {
            if(!strcmp(path,current->name)) {
                return current;
            }
        }
        current = current->next;
    }
    return 0;
}

void __tmpfs__find_dir(char *path) {
    char *last_slash = strrchr(path, '/');
    if (last_slash!= NULL && last_slash!= path) {
        *last_slash = '\0'; 
    } else {
        path[1] = '\0'; 
    }
}

char* __tmpfs__find_name(char* path) {
    char* last_slash = 0;
    while (*path) {
        if (*path == '/') {
            last_slash = path; 
        }
        path++;
    }
    if (last_slash) {
        return last_slash + 1; 
    }
    return path;
}

vfs::tmpfs_node_t* __tmpfs__symfind(char* path) {
    return __tmpfs__find(path); // symlinks will be handled by vfs
}

std::uint8_t __tmpfs__exists(char* path) {
    vfs::tmpfs_node_t* node = __tmpfs__find(path);
    if(node) {
        return 1;
    }
        
    return 0;
}

std::uint8_t __tmpfs__create_parent_dirs_by_default = 1; /* Used for ustar */

NodePoolBlock* node_pool_blocks_head = 0;

vfs::tmpfs_node_t* node_pool_current = 0;
size_t node_pool_offset = 0;
size_t node_pool_capacity = 0;

static void allocate_node_pool_block() {
    void* block = memory::pmm::_virtual::alloc(NODE_POOL_BLOCK_SIZE);
    if (!block) {
        return;
    }

    NodePoolBlock* new_block = (NodePoolBlock*)memory::pmm::_virtual::alloc(sizeof(NodePoolBlock));
    if (!new_block) {
        return;
    }
    new_block->block = reinterpret_cast<vfs::tmpfs_node_t*>(block);
    new_block->next = node_pool_blocks_head;
    node_pool_blocks_head = new_block;

    node_pool_current = new_block->block;
    node_pool_offset = 0;
    node_pool_capacity = NODE_POOL_BLOCK_SIZE / sizeof(vfs::tmpfs_node_t);;
}

static vfs::tmpfs_node_t* allocate_node_from_pool() {
    if (!node_pool_current || node_pool_offset >= node_pool_capacity) {
        allocate_node_pool_block();
        if (!node_pool_current) {
            return 0; 
        }
    }
    vfs::tmpfs_node_t* node = node_pool_current + node_pool_offset;
    node_pool_offset++;
    memset(node, 0, sizeof(vfs::tmpfs_node_t));
    return node;
}

std::uint64_t __tmpfs_ptr_id = 0;

std::int32_t __tmpfs__create(char* path,std::uint8_t type) {

    if(__tmpfs__exists(path))
        return EEXIST;
    
    char copy[2048];
    memset(copy,0,2048);
    memcpy(copy,path,strlen(path));

    if(path[0] == '\0')
        return EINVAL;

    __tmpfs__find_dir(copy);

    if(copy[0] == '\0') {
        copy[0] = '/';
        copy[1] = '\0';
    }

    if(!__tmpfs__exists(copy)) {
        if(!__tmpfs__create_parent_dirs_by_default)
            return ENOENT;
        else {
            __tmpfs__create(copy,TMPFS_TYPE_DIRECTORY);
        }
    }

    if(__tmpfs__find(copy)->type != VFS_TYPE_DIRECTORY)
        return ENOTDIR;

    vfs::tmpfs_node_t* node = head_node;

    while(node) {
        if(node->type == TMPFS_TYPE_NONE)
            break;
        node = node->next;
    }

    if(!node)
        node = (vfs::tmpfs_node_t*)allocate_node_from_pool();

    node->type = type;
    node->content = 0;
    node->size = 0;
    node->busy = 0;
    node->id = ++__tmpfs_ptr_id;

    node->create_time = getUnixTime();
    node->access_time = getUnixTime();
    
    memcpy(node->name,path,strlen(path));

    node->next = head_node;
    head_node = node;

    vfs::tmpfs_node_t* parent = __tmpfs__find(copy);
    if(parent->size <= DIRECTORY_LIST_SIZE) {
        ((std::uint64_t*)parent->content)[parent->size / 8] = (std::uint64_t)node;
        parent->size += 8;
    }

    if(node->type == VFS_TYPE_DIRECTORY) {
        node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(DIRECTORY_LIST_SIZE);
        node->size = 0;
    }

    return 0;
}

void __tmpfs__dealloc(vfs::tmpfs_node_t* node){
    if(!node->content)
        return;
    memory::pmm::_virtual::free(node->content);
}

std::uint8_t __tmpfs__dont_alloc_memory = 0;

std::int64_t __tmpfs__write(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size) {
    if (!path || !buffer || !size) { vfs::vfs::unlock();
        return -EBADF; }

    if (!__tmpfs__exists(path)) { vfs::vfs::unlock();
        return -ENOENT;  }

    vfs::tmpfs_node_t* node = __tmpfs__find(path);
    if (!node) { vfs::vfs::unlock();
        return -ENOENT; }

    if (node->type == TMPFS_TYPE_DIRECTORY) { vfs::vfs::unlock();
        return -EISDIR; }

    if (!fd) {
        __tmpfs__dealloc(node);
        node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(size);
        memcpy(node->content, buffer, size);
        node->size = size;
    } else {
        std::uint64_t offset = fd->offset;
        std::uint64_t new_size = offset + size;

        if (new_size > node->real_size || node->is_non_allocated) {
            alloc_t new_content0 = memory::pmm::_physical::alloc_ext(new_size);
            std::uint8_t* new_content = (std::uint8_t*)new_content0.virt;
            if (node->content) {
                memcpy(new_content, node->content, node->size); 
                if(!node->is_non_allocated)
                    __tmpfs__dealloc(node);
            }
            node->is_non_allocated = 0;
            node->content = new_content;
            node->real_size = new_content0.real_size;
        }

        node->size = new_size;

        node->access_time = getUnixTime();
        memcpy(node->content + offset, buffer, size);

        fd->offset += size;
    }

    vfs::vfs::unlock();
    return size; 
}

std::int64_t __tmpfs__read(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count) {
    if (!path || !buffer || !count) { vfs::vfs::unlock();
        return -EBADF; }

    if (!__tmpfs__exists(path)) { vfs::vfs::unlock();
        return -ENOENT; }

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    if (!node) { vfs::vfs::unlock();
        return -ENOENT; }

    if (node->type == TMPFS_TYPE_DIRECTORY) { vfs::vfs::unlock();
        return -EISDIR; }

    if (!fd) {
        std::uint64_t to_read = (count >= node->size) ? node->size : count;
        memcpy(buffer, node->content, to_read);
        vfs::vfs::unlock();
        return to_read;
    } else {
        std::uint64_t offset = fd->offset;
        if (offset >= node->size) {
            vfs::vfs::unlock();
            return 0;
        }
        std::uint64_t available = node->size - offset;
        std::uint64_t to_read = (count > available) ? available : count;

        memcpy(buffer, node->content + offset, to_read);
        fd->offset += to_read;
        
        vfs::vfs::unlock();
        return to_read;
    }
}

std::int32_t __tmpfs__open(userspace_fd_t* fd, char* path) {
    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return ENOENT;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    node->busy++;
    
    /* TmpFS doesnt need to change something in fd */

    return 0;
}

std::int32_t __tmpfs__var(userspace_fd_t* fd, char* path, std::uint64_t value, std::uint8_t request) {
    if(!path)
        return EFAULT;
    
    if(!__tmpfs__exists(path))
        return ENOENT;

    if(request == DEVFS_VAR_ISATTY)
        return ENOTTY;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    if(request & (1 << 7))
        node->vars[request & ~(1 << 7)] = value;
    else
        *(std::uint64_t*)value = node->vars[request & ~(1 << 7)];

    return 0;

}

std::int32_t __tmpfs__ls(userspace_fd_t* fd, char* path, vfs::dirent_t* out) {

    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return ENOENT;

again:
    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    if(node->type != VFS_TYPE_DIRECTORY)
        return ENOTDIR;

    if(fd->offset == node->size / 8) {
        out->d_reclen = 0;
        memset(out->d_name,0,sizeof(out->d_name));
        return 0; 
    }

    vfs::tmpfs_node_t* next = (vfs::tmpfs_node_t*)(((std::uint64_t*)node->content)[fd->offset]);

    if(next == 0) { // null pointer fix 
        out->d_reclen = 0;
        memset(out->d_name,0,sizeof(out->d_name));
        return 0; 
    }

    memset(out->d_name,0,sizeof(out->d_name));
    memcpy(out->d_name,__tmpfs__find_name(next->name),strlen(__tmpfs__find_name(next->name)));
    out->d_reclen = sizeof(vfs::dirent_t);
    fd->offset++;

    if(out->d_name[0] == 0)
        goto again;

    return 0;
}

std::int32_t __tmpfs__remove(userspace_fd_t* fd, char* path) {
    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return ENOENT;

    vfs::tmpfs_node_t* node = __tmpfs__find(path);
    if(node->type != TMPFS_TYPE_DIRECTORY) {

    } else {
        for(std::uint64_t i = 0;i < node->size / 8;i++) {
            __tmpfs__remove(fd,((vfs::tmpfs_node_t*)(((std::uint64_t*)node->content)[fd->offset / 8]))->name);
        }
    }

    memset(node->name,0,2048);
    __tmpfs__dealloc(node);
    return 0;
}

std::int32_t __tmpfs__touch(char* path) {
    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return __tmpfs__create(path,VFS_TYPE_FILE);

    return 0;

}

std::int32_t __tmpfs__stat(userspace_fd_t* fd, char* path, vfs::stat_t* out) {
    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return ENOENT;

    if(!out)
        return EINVAL;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return ENOENT;

    out->st_size = node->type == TMPFS_TYPE_DIRECTORY ? 0 : node->size;
    out->st_mode = node->type == TMPFS_TYPE_DIRECTORY ? S_IFDIR : S_IFREG;
    if(node->type == TMPFS_TYPE_SYMLINK) 
        out->st_mode = S_IFLNK;
    out->st_mode |= ((node->vars[0] & ~(S_IFDIR | S_IFREG | S_IFCHR | S_IFBLK | S_IFIFO | S_IFDIR | S_IFIFO | S_IFMT | S_IFLNK)) & 0xFFFFFFFF);
    out->st_uid = 0;
    out->st_gid = 0;
    out->st_blksize = 4096;
    out->st_ino = node->id;
    out->st_nlink = 0; // unimplemented
    out->st_dev = 0;
    out->st_rdev = 0;
    out->st_blocks = node->real_size / 4096;
    out->st_atim.tv_sec = node->access_time;
    out->st_mtim.tv_sec = node->access_time;
    out->st_ctim.tv_sec = node->create_time;
    out->st_atim.tv_nsec = 0;
    out->st_mtim.tv_nsec = 0;
    out->st_ctim.tv_nsec = 0;

    return 0;
}

std::int32_t __tmpfs__readlink(char* path, char* out, std::uint32_t out_len) {
    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return ENOENT;

    if(!out)
        return EINVAL;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return ENOENT;

    if(node->type != TMPFS_TYPE_SYMLINK)
        return EINVAL;

    if(node->size >= out_len) {
        Log::SerialDisplay(LEVEL_MESSAGE_INFO,"ur taking too long nodesize %d outlen %d",node->size,out_len);
        return EINVAL;
    }

    if(!node->content)
        return EINVAL; // symlink is not initializied now :(

    memcpy(out,node->content,node->size + 1);

    return 0;
}

std::int32_t __tmpfs__rename(char* path, char* new_path) {
    if(!__tmpfs__exists(path))
        return ENOENT;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return ENOENT;

    if(node->type == TMPFS_TYPE_DIRECTORY)
        return EFAULT; // not implemented in tmpfs

    if(node->busy != 0)
        return EBUSY;

    vfs::tmpfs_node_t* node2 = __tmpfs__symfind(new_path);

    if(node2) {

        if(node2->busy != 0)
            return EBUSY;

        if(node2->type == TMPFS_TYPE_DIRECTORY)
            return EFAULT; // not implemented in tmpfs

        userspace_fd_t fd;
        memset(&fd,0,sizeof(fd));
        __tmpfs__remove(&fd,new_path);
    }

    memset(node->name,0,2048);
    memcpy(node->name,new_path,strlen(new_path));
    return 0;

}

void __tmpfs__close(userspace_fd_t* fd, char* path) {
    if(!__tmpfs__exists(path))
        return;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return;

    if(node->busy > 0)
        node->busy--;

    return;
}

void vfs::tmpfs::mount(vfs_node_t* node) {
    head_node = (tmpfs_node_t*)memory::pmm::_virtual::alloc(4096);

    head_node->type = TMPFS_TYPE_DIRECTORY;
    head_node->id = 0;

    memcpy(head_node->name,"/",1);

    head_node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(DIRECTORY_LIST_SIZE);

    node->readlink = __tmpfs__readlink;
    node->create = __tmpfs__create;
    node->remove = __tmpfs__remove;
    node->rename = __tmpfs__rename;
    node->write = __tmpfs__write;
    node->touch = __tmpfs__touch;
    node->close = __tmpfs__close;
    node->read = __tmpfs__read;
    node->open = __tmpfs__open;
    node->stat = __tmpfs__stat;
    node->var = __tmpfs__var;
    node->ls = __tmpfs__ls;

}