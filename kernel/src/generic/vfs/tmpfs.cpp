
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
        if(current->type != TMPFS_TYPE_NONE) {
            if(!strcmp(path,current->name)) {
                return current;
            }
        }
        current = current->next;
    }
    return 0;
}

vfs::tmpfs_node_t* __tmpfs__symfind(char* path) {
    vfs::tmpfs_node_t* current = head_node;
    while(current) {
        if(current->type != TMPFS_TYPE_NONE && !strcmp(current->name,path)) {
            
            if(current->type != TMPFS_TYPE_SYMLINK)
                return current;
            
            std::uint8_t is_find = 1;
            vfs::tmpfs_node_t* try_find = current;
            while(is_find) {
                if(current->type == TMPFS_TYPE_SYMLINK && current->content) {
                    current = __tmpfs__symfind((char*)current->content);
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

std::int32_t __tmpfs__create(char* path,std::uint8_t type) {

    if(__tmpfs__exists(path))
        return -EFAULT;
    
    vfs::tmpfs_node_t* node = new vfs::tmpfs_node_t;
    node->type = type;
    node->content = 0;
    node->size = 0;
    
    memcpy(node->name,path,strlen(path));

    node->next = head_node;
    head_node = node;
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

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    if (!node)
        return -ENOENT;

    if (node->type != TMPFS_TYPE_FILE)
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

    if (node->type != TMPFS_TYPE_FILE)
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

void vfs::tmpfs::mount(vfs_node_t* node) {
    head_node = (tmpfs_node_t*)memory::pmm::_virtual::alloc(4096);

    head_node->content = 0;
    head_node->type = TMPFS_TYPE_DIRECTORY;

    memcpy(head_node->name,"/",1);

    node->create = __tmpfs__create;
    node->write = __tmpfs__write;
    node->read = __tmpfs__read;
}