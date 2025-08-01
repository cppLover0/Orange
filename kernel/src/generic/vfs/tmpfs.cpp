
#include <cstdint>
#include <generic/vfs/tmpfs.hpp>
#include <generic/vfs/vfs.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/heap.hpp>

#include <etc/libc.hpp>
#include <etc/errno.hpp>

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
    vfs::tmpfs_node_t* current = head_node;
    while(current) {
        if(current->type != TMPFS_TYPE_NONE && !strcmp(current->name,path) && current->name[0] != '\0') {
            
            if(current->type != TMPFS_TYPE_SYMLINK)
                return current;
            
            std::uint8_t is_find = 1;
            vfs::tmpfs_node_t* try_find = current;
            while(is_find) {
                if(current->type == TMPFS_TYPE_SYMLINK && current->content) {
                    char buf[2048];
                    vfs::resolve_path((const char*)current->content,current->name,buf,0,1);
                    return __tmpfs__symfind(buf);
                } else
                    is_find = 0;
            }
            return try_find;
        }
        current = current->next;
    }
    return 0;
}

std::uint8_t __tmpfs__exists(char* path) {
    vfs::tmpfs_node_t* node = __tmpfs__find(path);
    if(node) {
        return 1;
    }
        
    return 0;
}

std::uint8_t __tmpfs__create_parent_dirs_by_default = 1; /* Used for ustar */

std::int32_t __tmpfs__create(char* path,std::uint8_t type) {

    if(__tmpfs__exists(path))
        return EFAULT;
    
    char copy[2048];
    memset(copy,0,2048);
    memcpy(copy,path,strlen(path));
    __tmpfs__find_dir(copy);

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
        node = new vfs::tmpfs_node_t;

    node->type = type;
    node->content = 0;
    node->size = 0;
    
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

std::int64_t __tmpfs__write(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size) {
    if (!path || !buffer || !size)
        return -EBADF;

    if (!__tmpfs__exists(path))
        return -ENOENT;  

    vfs::tmpfs_node_t* node = __tmpfs__find(path);
    if (!node)
        return -ENOENT;

    if(node->content)
        node = __tmpfs__symfind(path);

    if (node->type == TMPFS_TYPE_DIRECTORY)
        return -EISDIR;

    if (!fd) {
        __tmpfs__dealloc(node);
        node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(size);
        memcpy(node->content, buffer, size);
        node->size = size;
    } else {
        std::uint64_t offset = fd->offset;
        std::uint64_t new_size = offset + size;

        if (new_size > node->size) {
            std::uint8_t* new_content = (std::uint8_t*)memory::pmm::_virtual::alloc(new_size);
            if (node->content) {
                memcpy(new_content, node->content, node->size); 
                __tmpfs__dealloc(node);
            }
            node->content = new_content;
            node->size = new_size;
        }

        memcpy(node->content + offset, buffer, size);

        fd->offset += size;
    }

    return size; 
}

std::int64_t __tmpfs__read(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count) {
    if (!path || !buffer || !count)
        return -EBADF;

    if (!__tmpfs__exists(path))
        return -ENOENT;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    if (!node)
        return -ENOENT;

    if (node->type == TMPFS_TYPE_DIRECTORY)
        return -EISDIR;

    if (!fd) {
        std::uint64_t to_read = (count >= node->size) ? node->size : count;
        memcpy(buffer, node->content, to_read);
        return to_read;
    } else {
        std::uint64_t offset = fd->offset;
        if (offset >= node->size) {
            return 0;
        }
        std::uint64_t available = node->size - offset;
        std::uint64_t to_read = (count > available) ? available : count;

        memcpy(buffer, node->content + offset, to_read);
        fd->offset += to_read;
        

        return to_read;
    }
}

std::int32_t __tmpfs__open(userspace_fd_t* fd, char* path) {
    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return ENOENT;
    
    /* TmpFS doesnt need to change something in fd */

    return 0;
}

std::int32_t __tmpfs__var(userspace_fd_t* fd, char* path, std::uint64_t value, std::uint8_t request) {
    if(!path)
        return EFAULT;
    
    if(!__tmpfs__exists(path))
        return ENOENT;

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

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    if(node->type != VFS_TYPE_DIRECTORY)
        return ENOTDIR;

    if(fd->offset == node->size / 8) {
        out->d_reclen = 0;
        return 0; 
    }

    vfs::tmpfs_node_t* next = (vfs::tmpfs_node_t*)(((std::uint64_t*)node->content)[fd->offset]);
    memset(out->d_name,0,sizeof(out->d_name));
    memcpy(out->d_name,__tmpfs__find_name(next->name),strlen(__tmpfs__find_name(next->name)));
    out->d_reclen = sizeof(vfs::dirent_t);
    fd->offset++;
    return 0;
}

std::int32_t __tmpfs__remove(userspace_fd_t* fd, char* path) {
    if(!path)
        return EFAULT;

    if(!__tmpfs__exists(path))
        return ENOENT;

    vfs::tmpfs_node_t* node = __tmpfs__find(path);
    if(node->type != TMPFS_TYPE_DIRECTORY) {
        node->type = TMPFS_TYPE_NONE;
    } else {
        for(std::uint64_t i = 0;i < node->size / 8;i++) {
            __tmpfs__remove(fd,((vfs::tmpfs_node_t*)(((std::uint64_t*)node->content)[fd->offset / 8]))->name);
        }
    }

    node->type = TMPFS_TYPE_NONE;
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
        return __tmpfs__create(path,VFS_TYPE_FILE);

    zeromem(out);

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    out->st_size = node->type == TMPFS_TYPE_DIRECTORY ? 0 : node->size;
    out->st_mode = node->type == TMPFS_TYPE_DIRECTORY ? S_IFDIR : S_IFCHR;
    return 0;
}

void vfs::tmpfs::mount(vfs_node_t* node) {
    head_node = (tmpfs_node_t*)memory::pmm::_virtual::alloc(4096);

    head_node->type = TMPFS_TYPE_DIRECTORY;

    memcpy(head_node->name,"/",1);

    head_node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(DIRECTORY_LIST_SIZE);

    node->create = __tmpfs__create;
    node->remove = __tmpfs__remove;
    node->write = __tmpfs__write;
    node->touch = __tmpfs__touch;
    node->read = __tmpfs__read;
    node->open = __tmpfs__open;
    node->stat = __tmpfs__stat;
    node->var = __tmpfs__var;
    node->ls = __tmpfs__ls;

}