
#include <generic/mm/pmm.hpp>
#include <generic/vfs/vfs.hpp>
#include <generic/vfs/tmpfs.hpp>
#include <generic/locks/spinlock.hpp>
#include <generic/vfs/devfs.hpp>

#include <arch/x86_64/syscalls/sockets.hpp>

#include <cstdint>
#include <etc/errno.hpp>

#include <etc/logging.hpp>

vfs::vfs_node_t vfs_nodes[512];
locks::spinlock* vfs_lock;

vfs::vfs_node_t* find_node(char* path) {
    std::uint64_t r = 0;
    vfs::vfs_node_t* match;
    for(int i = 0;i < 512;i++) {
        if(!strncmp(path,vfs_nodes[i].path,strlen(vfs_nodes[i].path))) {
            if(strlen(vfs_nodes[i].path) > r) {
                match = &vfs_nodes[i];
            }
        }
    }
    return match;
}

vfs::fifo_node_t* fifo_head = 0;

int is_fifo_exists(char* path) {
    vfs::fifo_node_t* current = fifo_head;
    while(current) {
        if(!strcmp(current->path,path))
            return 1;
        current = current->next;
    }
    return 0;
}

vfs::fifo_node_t* fifo_get(char* path) {

    vfs::fifo_node_t* current = fifo_head;
    while(current) {
        if(!strcmp(current->path,path))
            return current;
        current = current->next;
    }
    return 0;

}

char * __vfs__strtok(char *str, const char *delim) {
    static char *next = NULL;
    if (str) next = str;
    if (!next) return NULL;

    char *start = next;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    if (!*start) {
        next = NULL;
        return NULL;
    }

    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }

    if (*end) {
        *end = '\0';
        next = end + 1;
    } else {
        next = NULL;
    }

    return start;
}

void __vfs_symlink_resolve(char* path, char* out) {
    char buffer[2048];
    int e = vfs::vfs::readlink(path,buffer,2048);

    if(!vfs_lock->test())
        Log::Display(LEVEL_MESSAGE_WARN,"vfs_lock didnt set lock in symlink resolve");

    if(e == ENOSYS) 
        memcpy(out,path,strlen(path));

    if(e == EINVAL)
        memcpy(out,path,strlen(path));
    else if(e == 0) {
        
        char result[2048];
        memset(result,0,2048);
        vfs::resolve_path(buffer,path,result,0,1);
        __vfs_symlink_resolve(result,out);
    } else if(e == ENOENT) {
        memcpy(out,path,strlen(path));
        // maybe it wants directory symlink ?
        char path0[2048];
        memset(path0,0,2048);
        memcpy(path0,path,strlen(path));

        char result[2048];
        memset(result,0,2048);

        int c = 0;

        char* token = __vfs__strtok(path0, "/");
        
        /* Start building path from null and trying to find symlink */
        while(token) {
            
            result[c] = '/';
            c++;

            std::uint64_t mm = 0;
            mm = strlen(token);
            memcpy((char*)((std::uint64_t)result + c),token,mm);
            c += mm;
            result[c] = '\0';

            e = vfs::vfs::readlink(result,buffer,2048);
            if(e == 0) {
                char buffer2[2048];
                memset(buffer2,0,2048);
                vfs::resolve_path(buffer,result,buffer2,0,1);
                c = strlen(buffer2);
                buffer2[c++] = '/';
                memcpy(buffer2 + c, path + strlen(result) + 1, strlen(path) - strlen(result));
                __vfs_symlink_resolve(buffer2,out);
                return;
            } else if(e == ENOENT) {
                memcpy(out,path,strlen(path));
            }

            token = __vfs__strtok(0,"/");
        }
        memcpy(out,path,strlen(path));
    }
}

std::uint32_t vfs::vfs::create_fifo(char* path) {

    vfs_lock->lock();

    if(!path) {
        vfs_lock->unlock();
        return EINVAL;
    }

    if(is_fifo_exists(path)) {
        vfs_lock->unlock();
        return EEXIST;
    }

    fifo_node_t* current = fifo_head;
    while(current) {
        if(!current->is_used)
            break;
        current = current->next;
    }

    if(!current) {
        current = new fifo_node_t;
        current->next = fifo_head;
        fifo_head = current;
    }

    current->main_pipe = new pipe(0);
    current->main_pipe->connected_to_pipe_write = 2;

    memset(current->path,0,2048);
    memcpy(current->path,path,strlen(path));

    current->is_used = 1;

    vfs_lock->unlock();
    return 0;
}

std::int64_t vfs::vfs::write(userspace_fd_t* fd, void* buffer, std::uint64_t size) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out);
        memcpy(fd->path,out,strlen(out));
        fd->is_cached_path = 1;
    } else
        memcpy(out,fd->path,strlen(fd->path));


    if(is_fifo_exists(out)) {
        fifo_node_t* fifo = fifo_get(out);
        vfs_lock->unlock();
        asm volatile("sti");
        return fifo->main_pipe->write((const char*)buffer,size);
    }

    vfs_node_t* node = find_node(out);
    if(!node) { vfs::vfs::unlock();
        return -ENOENT; }

    char* fs_love_name = &out[0] + strlen(node->path) - 1;
    if(!node->write) { vfs::vfs::unlock();
        return -ENOSYS; }

    std::int64_t status = node->write(fd,fs_love_name,buffer,size);
    return status;
}

std::int64_t vfs::vfs::read(userspace_fd_t* fd, void* buffer, std::uint64_t count) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out);
        memcpy(fd->path,out,strlen(out));
        fd->is_cached_path = 1;
    } else
        memcpy(out,fd->path,strlen(fd->path));

    if(is_fifo_exists(out)) {
        fifo_node_t* fifo = fifo_get(out);
        vfs_lock->unlock();
        asm volatile("sti");
        return fifo->main_pipe->read(&fd->read_counter,(char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0);
    }

    vfs_node_t* node = find_node(out);
    if(!node) { vfs::vfs::unlock();
        return -ENOENT; }

    char* fs_love_name = out + strlen(node->path) - 1;
    if(!node->read) { vfs::vfs::unlock();
        return -ENOSYS; }

    std::int64_t status = node->read(fd,fs_love_name,buffer,count);
    return status;
}

std::int32_t vfs::vfs::create(char* path, std::uint8_t type) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    __vfs_symlink_resolve(path,out);

    if(sockets::is_exists(out) || is_fifo_exists(out)) {
        vfs_lock->unlock();
        return EEXIST;
    }

    vfs_node_t* node = find_node(out);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out + strlen(node->path) - 1;
    if(!node->create) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->create(out,type);
    vfs_lock->unlock();
    return status;
}

std::int32_t vfs::vfs::mmap(userspace_fd_t* fd, std::uint64_t* outp, std::uint64_t* outsz, std::uint64_t* outflags) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out);
        memcpy(fd->path,out,strlen(out));
        fd->is_cached_path = 1;
    } else
        memcpy(out,fd->path,strlen(fd->path));

    vfs_node_t* node = find_node(out);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out + strlen(node->path) - 1;
    if(!node->mmap) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->mmap(fd,fs_love_name,outp,outsz,outflags);
    vfs_lock->unlock();
    return status;
}

std::int32_t vfs::vfs::open(userspace_fd_t* fd) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out);
        memcpy(fd->path,out,strlen(out));
        fd->is_cached_path = 1;
    } else
        memcpy(out,fd->path,strlen(fd->path));

    if(is_fifo_exists(out)) {
        fifo_node_t* fifo = fifo_get(out);
        vfs_lock->unlock();
        return 0;
    }

    vfs_node_t* node = find_node(out);

    if(!node) { vfs::vfs::unlock();
        return ENOENT; }
    
    char* fs_love_name = out + strlen(node->path) - 1;
    if(!node->open) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->open(fd,fs_love_name);
    vfs_lock->unlock();
    return status;
}

std::int32_t vfs::vfs::remove(userspace_fd_t* fd) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    __vfs_symlink_resolve(fd->path,out);

    if(is_fifo_exists(out)) {
        fifo_node_t* fifo = fifo_get(out);
        memset(fifo->path,0,2048);
        fifo->is_used = 0;
        delete fifo->main_pipe;
        vfs_lock->unlock();
        return 0;
    }

    vfs_node_t* node = find_node(out);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }
    
    char* fs_love_name = out + strlen(node->path) - 1;
    if(!node->remove) {vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->remove(fd,fs_love_name);
    vfs_lock->unlock();
    return status;
}

std::int32_t vfs::vfs::ls(userspace_fd_t* fd, dirent_t* out) {
    vfs_lock->lock();

    char out0[2048];
    memset(out0,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out0);
        memcpy(fd->path,out0,strlen(out0));
        fd->is_cached_path = 1;
    } else
        memcpy(out0,fd->path,strlen(fd->path));

    vfs_node_t* node = find_node(out0);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out0 + strlen(node->path) - 1;
    if(!node->ls) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->ls(fd,fs_love_name,out);
    vfs_lock->unlock();
    return status;
} 

std::int32_t vfs::vfs::var(userspace_fd_t* fd, std::uint64_t value, std::uint8_t request) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out);
        memcpy(fd->path,out,strlen(out));
        fd->is_cached_path = 1;
    } else
        memcpy(out,fd->path,strlen(fd->path));

    vfs_node_t* node = find_node(out);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out + strlen(node->path) - 1;
    if(!node->var) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->var(fd,fs_love_name,value,request);
    vfs_lock->unlock();
    return status;
}

std::int32_t vfs::vfs::touch(char* path) {
    vfs_lock->lock();

    char out[2048];
    memset(out,0,2048);

    __vfs_symlink_resolve(path,out);

    if(sockets::is_exists(out) || is_fifo_exists(out)) {
        vfs_lock->unlock();
        return EEXIST;
    }

    vfs_node_t* node = find_node(out);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out + strlen(node->path) - 1;
    if(!node->touch) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->touch(fs_love_name);
    vfs_lock->unlock();
    return status;
}

std::int32_t vfs::vfs::nosym_stat(userspace_fd_t* fd, stat_t* out) {
    vfs_lock->lock();

    char out0[2048];
    memset(out0,0,2048);
    memcpy(out0,fd->path,strlen(fd->path));

    if(is_fifo_exists(out0)) {
        memset(out,0,sizeof(stat_t));
        out->st_mode |= S_IFIFO;
        vfs_lock->unlock();
        return 0;
    }

    if(sockets::is_exists(out0)) {
        memset(out,0,sizeof(stat_t));
        out->st_mode |= S_IFSOCK;
        vfs_lock->unlock();
        return 0;
    }

    vfs_node_t* node = find_node(out0);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out0 + strlen(node->path) - 1;
    if(!node->stat) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->stat(fd,fs_love_name,out);
    vfs_lock->unlock();
    return status;
} 

std::int32_t vfs::vfs::stat(userspace_fd_t* fd, stat_t* out) {
    vfs_lock->lock();

    char out0[2048];
    memset(out0,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out0);
        memcpy(fd->path,out0,strlen(out0));
        fd->is_cached_path = 1;
    } else
        memcpy(out0,fd->path,strlen(fd->path));

    if(is_fifo_exists(out0)) {
        memset(out,0,sizeof(stat_t));
        out->st_mode |= S_IFIFO;
        vfs_lock->unlock();
        return 0;
    }

    if(sockets::is_exists(out0)) {
        memset(out,0,sizeof(stat_t));
        out->st_mode |= S_IFSOCK;
        vfs_lock->unlock();
        return 0;
    }

    vfs_node_t* node = find_node(out0);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out0 + strlen(node->path) - 1;
    if(!node->stat) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int32_t status = node->stat(fd,fs_love_name,out);
    vfs_lock->unlock();
    return status;
} 

std::int64_t vfs::vfs::ioctl(userspace_fd_t* fd, unsigned long req, void *arg, int *res) {
    vfs_lock->lock();

    char out0[2048];
    memset(out0,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out0);
        memcpy(fd->path,out0,strlen(out0));
        fd->is_cached_path = 1;
    } else
        memcpy(out0,fd->path,strlen(fd->path));

    vfs_node_t* node = find_node(out0);
    if(!node) { vfs::vfs::unlock();
        return ENOENT; }

    char* fs_love_name = out0 + strlen(node->path) - 1;
    if(!node->ioctl) { vfs::vfs::unlock();
        return ENOSYS; }

    std::int64_t status = node->ioctl(fd,fs_love_name,req,arg,res);
    vfs_lock->unlock();
    return status;
}

void vfs::vfs::close(userspace_fd_t* fd) {
    vfs_lock->lock();

    char out0[2048];
    memset(out0,0,2048);

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out0);
        memcpy(fd->path,out0,strlen(out0));
        fd->is_cached_path = 1;
    } else
        memcpy(out0,fd->path,strlen(fd->path));

    if(is_fifo_exists(out0)) {
        fifo_node_t* fifo = fifo_get(out0);
        fifo->main_pipe->fifoclose();
        vfs_lock->unlock();
        return;
    }

    vfs_node_t* node = find_node(out0);
    if(!node) { vfs::vfs::unlock();
        return; }

    char* fs_love_name = out0 + strlen(node->path) - 1;
    if(!node->close) { vfs::vfs::unlock();
        return; }

    node->close(fd,fs_love_name);
    vfs_lock->unlock();
    return;
}

std::int32_t vfs::vfs::readlink(char* path, char* out, std::uint32_t out_len) {
    if(is_fifo_exists(path)) {
        return EINVAL;
    } else if(sockets::is_exists(path)) {
        return EINVAL;
    }

    if(path[0] == '\0') {
        path[0] = '/';
        path[1] = '\n';
    }

    vfs_node_t* node = find_node(path);
    if(!node) { 
        return ENOENT; }

    char* fs_love_name = path + strlen(node->path) - 1;
    if(!node->readlink) { 
        return EINVAL; }

    return node->readlink(fs_love_name,out,out_len);
}

std::int64_t vfs::vfs::poll(userspace_fd_t* fd, int operation_type) {

    vfs_lock->lock();
    char out0[2048];
    memset(out0,0,2048);

    if(!fd) {
        vfs_lock->unlock();
        return 0;
    }

    if(fd->state == USERSPACE_FD_STATE_SOCKET) {
        if(fd->is_listen) {
                uint64_t counter = 0;
                if(operation_type == POLLIN)
                    counter = sockets::find(fd->path)->socket_counter;
                vfs_lock->unlock();
                return counter; 
            }
    }

    if(!fd->is_cached_path) {
        __vfs_symlink_resolve(fd->path,out0);
        memcpy(fd->path,out0,strlen(out0));
        fd->is_cached_path = 1;
    } else
        memcpy(out0,fd->path,strlen(fd->path));

    if(is_fifo_exists(out0)) {
        fifo_node_t* fifo = fifo_get(out0);
        std::int64_t ret = 0;
        switch (operation_type)
        {
        
        case POLLIN:
            ret = fifo->main_pipe->read_counter;
            if(fifo->main_pipe->size != 0 && fd->read_counter == -1)
                ret = 0;
            if(fifo->main_pipe->size != 0 && ret == fd->read_counter) {
                fifo->main_pipe->read_counter++;
                ret = fifo->main_pipe->read_counter;
            }
            break;
        
        case POLLOUT:
            ret = fifo->main_pipe->write_counter;
            if(fifo->main_pipe->write_counter == ret && fifo->main_pipe->size != fifo->main_pipe->total_size) {
                fifo->main_pipe->write_counter++;
                ret = fifo->main_pipe->write_counter;
            }
            break;
        
        default:
            break;
        }
        vfs_lock->unlock();
        return ret;
    } else if(fd->state == USERSPACE_FD_STATE_SOCKET) {
        std::int64_t ret = 0;
        switch (operation_type)
        {
        
        case POLLIN:
            ret = fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? fd->write_socket_pipe->read_counter : fd->read_socket_pipe->read_counter;
            if(fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER)
                if(fd->write_socket_pipe->size != 0 && fd->read_counter == -1)
                    ret = 0;
            if(fd->other_state == USERSPACE_FD_OTHERSTATE_SLAVE)
                if(fd->read_socket_pipe->size != 0 && fd->read_counter == -1)
                    ret = 0;

            if(fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
                if(fd->write_socket_pipe->read_counter == ret && fd->write_socket_pipe->size != 0) {
                    fd->write_socket_pipe->read_counter++;
                }
            }

            if(fd->other_state == USERSPACE_FD_OTHERSTATE_SLAVE) {
                if(fd->read_socket_pipe->read_counter == ret && fd->read_socket_pipe->size != 0) {
                    fd->read_socket_pipe->read_counter++;
                }
            }

            ret = fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? fd->write_socket_pipe->read_counter : fd->read_socket_pipe->read_counter;
            
                
            break;
        
        case POLLOUT:
            ret = fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? fd->read_socket_pipe->write_counter : fd->write_socket_pipe->write_counter;
            
            if(fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER) {
                if(fd->read_socket_pipe->write_counter == ret && fd->read_socket_pipe->size != fd->read_socket_pipe->total_size) {
                    fd->read_socket_pipe->write_counter++;
                }
            }

            if(fd->other_state == USERSPACE_FD_OTHERSTATE_SLAVE) {
                if(fd->write_socket_pipe->write_counter == ret && fd->write_socket_pipe->size != fd->write_socket_pipe->total_size) {
                    fd->write_socket_pipe->write_counter++;
                }
            }

            ret = fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? fd->read_socket_pipe->write_counter : fd->write_socket_pipe->write_counter;
            
            break;
        
        default:
            break;
        }

        vfs_lock->unlock();
        return ret;
    } else if(fd->state == USERSPACE_FD_STATE_PIPE) {

        std::int64_t ret = 0;
        switch (operation_type)
        {
        
        case POLLIN:
            ret = fd->pipe->read_counter;
            if(fd->pipe->read_counter == -1 && fd->pipe->size != 0)
                ret = 0;
            if(fd->pipe->read_counter == ret && fd->pipe->size != 0) {
                fd->pipe->read_counter++;
                ret = fd->pipe->read_counter;
            }
            break;
        
        case POLLOUT:
            ret = fd->pipe->write_counter;
            if(fd->pipe->write_counter == ret && fd->pipe->size != fd->pipe->total_size) {
                fd->pipe->write_counter++;
                ret = fd->pipe->write_counter;
            }
            break;
        
        default:
            break;
        }
        vfs_lock->unlock();
        return ret;
    }

    vfs_node_t* node = find_node(out0);
    if(!node) { vfs::vfs::unlock();
        return -ENOENT; }

    char* fs_love_name = out0 + strlen(node->path) - 1;
    if(!node->poll) { vfs::vfs::unlock();
        return -ENOSYS ; }

    std::int64_t ret = node->poll(fd,fs_love_name,operation_type);
    vfs_lock->unlock();
    return ret;
}


void vfs::vfs::unlock() {
    vfs_lock->unlock();
}

void vfs::vfs::lock() {
    vfs_lock->lock();
}

void vfs::vfs::init() {
    memset(vfs_nodes,0,sizeof(vfs_nodes));

    vfs_lock = new locks::spinlock;
    vfs_lock->unlock();
    
    tmpfs::mount(&vfs_nodes[0]);
    memcpy(vfs_nodes[0].path,"/",1);
    Log::Display(LEVEL_MESSAGE_OK,"TmpFS initializied\n");

    devfs::mount(&vfs_nodes[1]);
    memcpy(vfs_nodes[1].path,"/dev/",5);
    Log::Display(LEVEL_MESSAGE_OK,"DevFS initializied\n");
}
