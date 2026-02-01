
#include <cstdint>
#include <generic/vfs/tmpfs.hpp>
#include <generic/vfs/vfs.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/mm/heap.hpp>

#include <etc/etc.hpp>

#include <etc/hash.hpp>

#include <etc/libc.hpp>
#include <etc/errno.hpp>

#include <drivers/cmos.hpp>
#include <drivers/tsc.hpp>

#include <etc/logging.hpp>

std::uint64_t __tmpfs_ptr_id = 0;

vfs::tmpfs_node_t* head_node = 0;
vfs::tmpfs_node_t* root_node = 0; 

vfs::tmpfs_node_t* __tmpfs__find(const char* path) {

    extern locks::spinlock* vfs_lock;

    if(path[0] == '/' && path[1] == '\0')
        return root_node;

    if(path[0] == '\0')
        return root_node;

    char built_path[2048];
    char copied_path[2048];
    memcpy(copied_path,path,strlen(path) + 1);
    memset(built_path,0,2048);
    char* next = 0;
    char* token = __vfs__strtok(&next, copied_path, "/");

    std::uint64_t need_hash = hash_fnv1(path);
    std::uint64_t ptr = 0;
        
    vfs::tmpfs_node_t* current = root_node;
    while(token) {

        built_path[ptr] = '/';
        ptr++;

        memcpy((char*)((uint64_t)built_path + ptr),token,strlen(token));
        ptr += strlen(token);
        built_path[ptr] = '\0';

        std::uint64_t rehashed = hash_fnv1(built_path);
        if(current->type != VFS_TYPE_DIRECTORY) {
            return 0;
        } else if(current->path_hash == need_hash)
            return current;

        int is_cont = 1;

        std::uint64_t* cont = (std::uint64_t*)current->content;
        if(current->content) {
            for(std::uint64_t i = 0; (i < (current->size / 8)) && is_cont; i++) { 
                vfs::tmpfs_node_t* chld = (vfs::tmpfs_node_t*)cont[i];
                if(chld) {
                    if(chld->path_hash == rehashed) {
                        current = chld;
                        is_cont = 0;
                        goto end;
                    } 
                }
            }
        }
end:
        if(is_cont)
            return 0;
        
        if(current->path_hash == need_hash)
            return current;

        token = __vfs__strtok(&next,0,"/");
    }

    if(current->path_hash == need_hash)
        return current;

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
    return (vfs::tmpfs_node_t*)memory::pmm::_virtual::alloc(4096);
}

int __tmpfs__is_busy(vfs::tmpfs_node_t* node) {

    if(!node)
        return 0;

    if(node->busy != 0)
        return 1;

    for(std::uint64_t i = 0; i < (node->size / 8); i++) {
        if(node->content[i] != 0) {
            vfs::tmpfs_node_t* chld_node = (vfs::tmpfs_node_t*)(((std::uint64_t*)node->content)[i]);
            if(chld_node->busy != 0)
                return 1;

            if(chld_node->type == TMPFS_TYPE_DIRECTORY) {
                int c = __tmpfs__is_busy(chld_node);
                if(c == 1)
                    return c;
            }
 
        }
    }

    return 0;
}

std::int32_t __tmpfs__create(char* path,std::uint8_t type) {

    if(__tmpfs__exists(path))
        return -EEXIST;
    
    char copy[2048];
    memset(copy,0,2048);
    memcpy(copy,path,strlen(path));

    if(path[0] == '\0')
        return -EINVAL;

    __tmpfs__find_dir(copy);

    if(copy[0] == '\0') {
        copy[0] = '/';
        copy[1] = '\0';
    }

    if(!__tmpfs__exists(copy)) {
        if(!__tmpfs__create_parent_dirs_by_default)
            return -ENOENT;
        else {
            __tmpfs__create(copy,TMPFS_TYPE_DIRECTORY);
        }
    }

    if(__tmpfs__find(copy)->type != VFS_TYPE_DIRECTORY)
        return -ENOTDIR;

    vfs::tmpfs_node_t* node = head_node;

    int is_success_find = 0;

    while(node) {
        if(node->name[0] == '\0') {
            vfs::tmpfs_node_t* nex = node->next;
            memset(node,0,sizeof(vfs::tmpfs_node_t));
            node->next = nex;
            is_success_find = 1;
            break;
        }

        node = node->next;
    }

    if(!node)
        node = (vfs::tmpfs_node_t*)memory::pmm::_virtual::alloc(4096);

    node->type = type;
    node->content = 0;
    node->size = 0;
    node->busy = 0;
    node->id = ++__tmpfs_ptr_id;

    node->create_time = getUnixTime();
    node->access_time = getUnixTime();
    
    memcpy(node->name,path,strlen(path));

    node->path_hash = hash_fnv1(node->name);

    if(!is_success_find) {
        node->next = head_node;
        head_node = node;
    }

    vfs::tmpfs_node_t* parent = __tmpfs__find(copy);

    if(!(parent->size <= parent->real_size)) {
        // overflow ! increase size
        parent->content = (std::uint8_t*)memory::pmm::_virtual::realloc(parent->content,parent->real_size,parent->real_size + PAGE_SIZE);
        parent->real_size += PAGE_SIZE;
    }

    if(parent->size <= parent->real_size) {
        // first: find free entry

        std::uint64_t* free_entry = 0;
        for(std::uint64_t i = 0; i < (parent->size / 8); i++) {
            if(((std::uint64_t*)parent->content)[i] == 0) {
                free_entry = &(((std::uint64_t*)parent->content)[i]);
                break;
            }
        }

        if(!free_entry) {
            free_entry = &((std::uint64_t*)parent->content)[parent->size / 8];
            parent->size += 8;
        }

        *free_entry = (std::uint64_t)node;
        
    }

    if(node->type == VFS_TYPE_DIRECTORY) {
        node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(DIRECTORY_LIST_SIZE);
        node->real_size = DIRECTORY_LIST_SIZE;
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

        vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if (!node) { vfs::vfs::unlock();
        return -ENOENT;  }

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
    if (!path || !buffer) { vfs::vfs::unlock();
        return -EBADF; }

    if(!count) { vfs::vfs::unlock();
        return 0; }

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if (!node) { vfs::vfs::unlock();
        return -ENOENT; }

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

void __tmpfs__opt_create_and_write(char* path, int type, char* content, std::uint64_t content_len, int chmod) {
    
    char copy[2048];
    memcpy(copy,path,strlen(path) + 1);

    if(path[0] == '\0')
        return;

    __tmpfs__find_dir(copy);

    if(copy[0] == '\0') {
        copy[0] = '/';
        copy[1] = '\0';
    }

    if(!__tmpfs__exists(copy)) {
        if(!__tmpfs__create_parent_dirs_by_default)
            return;
        else {
            int status = __tmpfs__create(copy,TMPFS_TYPE_DIRECTORY);
            assert(__tmpfs__exists(copy),"error creating parent %s, status %d",copy,status);
        }
    }

    vfs::tmpfs_node_t* node = 0;

    if(!node)
        node = (vfs::tmpfs_node_t*)memory::pmm::_virtual::alloc(4096);

    node->type = type;
    node->content = 0;
    node->size = 0;
    node->busy = 0;
    node->id = ++__tmpfs_ptr_id;

    node->create_time = getUnixTime();
    node->access_time = getUnixTime();
    
    memcpy(node->name,path,strlen(path) + 1);

    node->path_hash = hash_fnv1(node->name);

    node->next = head_node;
    head_node = node;

    vfs::tmpfs_node_t* parent = __tmpfs__find(copy);

    parent = __tmpfs__find(copy);

    assert(parent,"parent is null %s (%d)",copy,__tmpfs__create_parent_dirs_by_default);

    if(!parent->content) {
        node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(DIRECTORY_LIST_SIZE);
        node->real_size = DIRECTORY_LIST_SIZE;
        node->size = 0;
    }

    if(!(parent->size <= parent->real_size)) {
        // overflow ! increase size
        parent->content = (std::uint8_t*)memory::pmm::_virtual::realloc(parent->content,parent->real_size,parent->real_size + PAGE_SIZE);
        parent->real_size += PAGE_SIZE;
    }

    if(parent->size <= parent->real_size) {
        std::uint64_t* free_entry = &((std::uint64_t*)parent->content)[parent->size / 8];
        parent->size += 8;

        *free_entry = (std::uint64_t)node;
    } 

    if(node->type == VFS_TYPE_DIRECTORY) {
        node->content = (std::uint8_t*)memory::pmm::_virtual::alloc(DIRECTORY_LIST_SIZE);
        node->real_size = DIRECTORY_LIST_SIZE;
        node->size = 0;
    } else {
        alloc_t new_content0 = memory::pmm::_physical::alloc_ext(content_len);
        std::uint8_t* new_content = (std::uint8_t*)new_content0.virt;
        if (node->content) {
            memcpy(new_content, node->content, node->size); 
            if(!node->is_non_allocated)
                __tmpfs__dealloc(node);
        }
        node->is_non_allocated = 0;
        node->content = new_content;
        node->real_size = new_content0.real_size;
        node->size = content_len;
        memcpy(node->content,content,content_len);
    }

    node->vars[TMPFS_VAR_CHMOD] = chmod;

    return;

}

std::int32_t __tmpfs__open(userspace_fd_t* fd, char* path) {
    if(!path)
        return EFAULT;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return ENOENT;
    
    node->busy++;
    
    /* TmpFS doesnt need to change something in fd */

    return 0;
}

std::int32_t __tmpfs__var(userspace_fd_t* fd, char* path, std::uint64_t value, std::uint8_t request) {
    if(!path)
        return EFAULT;

        vfs::tmpfs_node_t* node = __tmpfs__symfind(path);
    
    if(!node)
        return ENOENT;

    if(request == DEVFS_VAR_ISATTY)
        return ENOTTY;

    if(request & (1 << 7))
        node->vars[request & ~(1 << 7)] = value;
    else
        *(std::uint64_t*)value = node->vars[request & ~(1 << 7)];

    return 0;

}

std::int32_t __tmpfs__ls(userspace_fd_t* fd, char* path, vfs::dirent_t* out) {

    if(!path)
        return EFAULT;

vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return ENOENT;

again:
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

    std::uint64_t mod = next->type == TMPFS_TYPE_DIRECTORY ? DT_DIR : DT_REG;
    if(next->type == TMPFS_TYPE_SYMLINK) 
        mod = DT_LNK;

    out->d_reclen = sizeof(vfs::dirent_t);
    out->d_ino = next->id;
    out->d_off = 0;
    out->d_type = mod;
    fd->offset++;

    if(out->d_name[0] == 0)
        goto again;

    return 0;
}

std::int32_t __tmpfs__remove(userspace_fd_t* fd, char* path) {
    if(!path)
        return -EFAULT;

    vfs::tmpfs_node_t* node = __tmpfs__find(path);

    if(!node)
        return -ENOENT;

    char copy[2048];
    memset(copy,0,2048);
    memcpy(copy,path,strlen(path));

    if(path[0] == '\0')
        return -EINVAL;

    __tmpfs__find_dir(copy);

    if(copy[0] == '\0') {
        copy[0] = '/';
        copy[1] = '\0';
    }

    vfs::tmpfs_node_t* parent = __tmpfs__find(copy);

    if(__tmpfs__is_busy(node))
        return -EBUSY;

    if(node->type != TMPFS_TYPE_DIRECTORY) {

    } else {
        for(std::uint64_t i = 0;i < node->size / 8;i++) {
            __tmpfs__remove(fd,((vfs::tmpfs_node_t*)(((std::uint64_t*)node->content)[fd->offset / 8]))->name);
        }
    }

    if(parent) {
        for(std::uint64_t i = 0; i < (node->size / 8);i++) {
            std::uint64_t* entry = &(((std::uint64_t*)node->content)[i]);
            *entry = 0;
        }
    }

    memset(node->name,0,2048);
    __tmpfs__dealloc(node);

    return 0;
}

std::int32_t __tmpfs__touch(char* path, int mode) {
    if(!path)
        return EFAULT;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node) {
        int status =  __tmpfs__create(path,VFS_TYPE_FILE);
        node = __tmpfs__symfind(path);
        if(node)
            node->vars[TMPFS_VAR_CHMOD] |= (mode & ~(S_IFDIR | S_IFREG | S_IFCHR | S_IFBLK | S_IFIFO | S_IFDIR | S_IFIFO | S_IFMT | S_IFLNK));
    }

    return 0;

}

#define STATX_BASIC_STATS 0x7ff
#define STATX_BTIME 0x800



std::int32_t __tmpfs__statx(userspace_fd_t* fd, char* path, int flags, int mask, statx_t* out) {
    if(!path)
        return -EFAULT;

    if(!__tmpfs__exists(path))
        return -ENOENT;

    if(!out)
        return -EINVAL;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    memset(out,0,sizeof(statx_t));

    out->stx_mask = STATX_BASIC_STATS | STATX_BTIME;
    out->stx_blksize = 4096;
    out->stx_nlink = 0;
    out->stx_uid = 0;
    out->stx_gid =  0;
    out->stx_mode = 0;
    switch(node->type) {
        case TMPFS_TYPE_DIRECTORY:
            out->stx_mode |= S_IFDIR;
            break;
        case TMPFS_TYPE_FILE:
            out->stx_mode |= S_IFREG;
            break;
        case TMPFS_TYPE_SYMLINK:
            out->stx_mode |= S_IFLNK;
            break;
        default:
            out->stx_mode |= S_IFREG;
            break;
    }
    out->stx_ino = node->id;
    out->stx_size = node->size;
    out->stx_blocks = node->real_size / 512;
    out->stx_atime.tv_sec = node->access_time;
    out->stx_atime.tv_nsec = 0;
    out->stx_btime.tv_sec = node->create_time;
    out->stx_btime.tv_nsec = 0;
    out->stx_ctime.tv_sec = node->access_time;
    out->stx_ctime.tv_nsec = 0;
    out->stx_mtime.tv_sec = node->access_time;
    out->stx_mtime.tv_nsec = 0;
    out->stx_rdev_major = 0;
    out->stx_rdev_minor = 0;
    out->stx_dev_major = 0;
    out->stx_dev_minor = 0;
    return 0;
}

std::int32_t __tmpfs__stat(userspace_fd_t* fd, char* path, vfs::stat_t* out) {
    if(!path)
        return EFAULT;

vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return ENOENT;

    if(!out)
        return EINVAL;


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

// note for me never touch this in future
std::int32_t __tmpfs__readlink(char* path, char* out, std::uint32_t out_len) {
    if(!path)
        return EFAULT;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return ENOENT;

    if(!out)
        return EINVAL;

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

void __tmpfs_build_newfile(char* path, char* new_path, int path_start_len, char* result) {
    char temp_path[2048]; // /usr/bin -> /bin
    memset(temp_path,0,2048);
    memcpy(temp_path,path + path_start_len,strlen(path + path_start_len));
    memcpy(result,new_path,strlen(new_path)); 
    memcpy(result + strlen(new_path),temp_path,strlen(temp_path) + 1);
    if(result[0] == '/' && result[1] == '/') {
        // fix
        memset(temp_path,0,2048);
        memcpy(temp_path,result + 1, strlen(result + 1));
        memcpy(result,temp_path,strlen(temp_path) + 1);
    }
}

void __tmpfs__rename_dirents(char* path, char* new_path, int path_start_len) {
    if(!__tmpfs__exists(path))
        return;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return;

    if(node->busy != 0)
        return;

    char old_node_path[2048];
    memcpy(old_node_path,node->name,strlen(node->name) + 1);

    if(node->type == TMPFS_TYPE_DIRECTORY) {

        std::uint64_t* cont = (std::uint64_t*)node->content;
        for(std::uint64_t i = 0; i < (node->size / 8); i++) {
            if(cont[i] != 0) {
                vfs::tmpfs_node_t* chld_node = (vfs::tmpfs_node_t*)cont[i];
                char old_name[2048];
                memset(old_name,0,2048);
                memcpy(old_name,chld_node->name,strlen(chld_node->name) + 1);
                memset(chld_node->name,0,sizeof(chld_node->name));
                __tmpfs_build_newfile(old_name,node->name,path_start_len,chld_node->name);
                chld_node->path_hash = hash_fnv1(chld_node->name);
                DEBUG(1,"new_path %s\n",chld_node->name);
                if(chld_node->type == TMPFS_TYPE_DIRECTORY) {
                    __tmpfs__rename_dirents(chld_node->name,new_path,path_start_len);
                }
            }
        }
    }
    return;
}

std::int32_t __tmpfs__rename(char* path, char* new_path) {
    if(!__tmpfs__exists(path))
        return -ENOENT;

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return -ENOENT;

    if(node->busy != 0)
        return -EBUSY;

    if(__tmpfs__is_busy(node))
        return -EBUSY;

    char old_node_path[2048];
    memcpy(old_node_path,node->name,strlen(node->name) + 1);

    vfs::tmpfs_node_t* node2 = __tmpfs__symfind(new_path);

    if(node2) {

        if(node2->busy != 0)
            return -EBUSY;

        if(node2->type == TMPFS_TYPE_DIRECTORY) {
            // man says it shoudl return error only if its non empty
            int is_non_empty = 1;
            for(std::uint64_t i = 0;i < node2->size / 8; i++) {
                if((node2->content[i]))
                    return ENOTEMPTY;
            }
        }

        userspace_fd_t fd;
        memset(&fd,0,sizeof(fd));
        __tmpfs__remove(&fd,new_path);
    }

    char copy[2048];
    memset(copy,0,2048);
    memcpy(copy,path,strlen(path));

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
        return -ENOTDIR;

    vfs::tmpfs_node_t* parent = __tmpfs__find(copy);
    if(parent->size <= DIRECTORY_LIST_SIZE) {
        // first: find free entry

        std::uint64_t* free_entry = 0;
        for(std::uint64_t i = 0; i < (parent->size / 8); i++) {
            if(((std::uint64_t*)parent->content)[i] == (std::uint64_t)node) {
                free_entry = &(((std::uint64_t*)parent->content)[i]);
                *free_entry = 0;
            }
        }
        
    }

    int path_start_len = strlen(node->name);
    if(node->name[0] == '/' && node->name[1] == '\0')
        path_start_len = 0;

    memset(node->name,0,2048);
    memcpy(node->name,new_path,strlen(new_path));

    node->path_hash = hash_fnv1(node->name);

    if(node->type == TMPFS_TYPE_DIRECTORY) {

        std::uint64_t* cont = (std::uint64_t*)node->content;
        for(std::uint64_t i = 0; i < (node->size / 8); i++) {
            if(cont[i] != 0) {
                vfs::tmpfs_node_t* chld_node = (vfs::tmpfs_node_t*)cont[i];
                char old_name[2048];
                memset(old_name,0,2048);
                memcpy(old_name,chld_node->name,strlen(chld_node->name) + 1);
                memset(chld_node->name,0,sizeof(chld_node->name));
                __tmpfs_build_newfile(old_name,node->name,path_start_len,chld_node->name);
                chld_node->path_hash = hash_fnv1(chld_node->name);
                DEBUG(1,"new_path %s\n",chld_node->name);
                if(chld_node->type == TMPFS_TYPE_DIRECTORY) {
                    __tmpfs__rename_dirents(chld_node->name,new_path,path_start_len);
                }
            }
        }
    }
 
    memset(copy,0,2048);
    memcpy(copy,path,strlen(path));

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

    parent = __tmpfs__find(copy);
    if(parent->size <= DIRECTORY_LIST_SIZE) {
        // first: find free entry

        std::uint64_t* free_entry = 0;
        for(std::uint64_t i = 0; i < (parent->size / 8); i++) {
            if(((std::uint64_t*)parent->content)[i] == 0) {
                free_entry = &(((std::uint64_t*)parent->content)[i]);
                break;
            }
        }

        if(!free_entry) {
            free_entry = &((std::uint64_t*)parent->content)[parent->size / 8];
            parent->size += 8;
        }

        *free_entry = (std::uint64_t)node;
        
    }

    return 0;

}

void __tmpfs__close(userspace_fd_t* fd, char* path) {

    vfs::tmpfs_node_t* node = __tmpfs__symfind(path);

    if(!node)
        return;

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

    root_node = head_node;

    memcpy(head_node->name,"/",1);

    head_node->path_hash = hash_fnv1(head_node->name);

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

    node->statx = __tmpfs__statx;

    node->opt_create_and_write = __tmpfs__opt_create_and_write;

}