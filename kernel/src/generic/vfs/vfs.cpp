
#include <generic/mm/pmm.hpp>
#include <generic/vfs/vfs.hpp>
#include <generic/vfs/tmpfs.hpp>
#include <generic/locks/spinlock.hpp>
#include <generic/vfs/devfs.hpp>

#include <cstdint>
#include <etc/errno.hpp>

#include <etc/logging.hpp>

vfs::vfs_node_t vfs_nodes[512];
locks::spinlock* vfs_lock;

vfs::vfs_node_t* find_node(char* path) {
    for(int i = 0;i < 512;i++) {
        if(!strncmp(path,vfs_nodes[i].path,strlen(vfs_nodes[i].path))) {
            return &vfs_nodes[i];
        }
    }
    return 0;
}

std::int64_t vfs::vfs::write(userspace_fd_t* fd, void* buffer, std::uint64_t size) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return -ENOENT;

    char* fs_love_name = &fd->path[0] + strlen(node->path) - 1;
    if(!node->write)
        return -ENOSYS;

    std::int64_t status = node->write(fd,fs_love_name,buffer,size);
    if(!(status & (1 << 63)))
        vfs_lock->unlock();
    else if(status & (1 << 63))
        status &= ~(1 << 63);
    return status;
}

std::int64_t vfs::vfs::read(userspace_fd_t* fd, void* buffer, std::uint64_t count) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return -ENOENT;

    char* fs_love_name = fd->path + strlen(node->path) - 1;
    if(!node->read)
        return -ENOSYS;

    std::int64_t status = node->read(fd,fs_love_name,buffer,count);
    if(!(status & (1 << 63)))
        vfs_lock->unlock();
    else if(status & (1 << 63))
        status &= ~(1 << 63);
    return status;
}

std::int32_t vfs::vfs::create(char* path, std::uint8_t type) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(path);
    if(!node)
        return ENOENT;

    char* fs_love_name = path + strlen(node->path) - 1;
    if(!node->create)
        return ENOSYS;

    std::int32_t status = node->create(path,type);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
}

std::int32_t vfs::vfs::mmap(userspace_fd_t* fd, std::uint64_t* outp, std::uint64_t* outsz ) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return ENOENT;

    char* fs_love_name = fd->path + strlen(node->path) - 1;
    if(!node->mmap)
        return ENOSYS;

    std::int32_t status = node->mmap(fd,fs_love_name,outp,outsz);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
}

std::int32_t vfs::vfs::open(userspace_fd_t* fd) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return ENOENT;
    
    char* fs_love_name = fd->path + strlen(node->path) - 1;
    if(!node->open)
        return ENOSYS;

    std::int32_t status = node->open(fd,fs_love_name);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
}

std::int32_t vfs::vfs::remove(userspace_fd_t* fd) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return ENOENT;
    
    char* fs_love_name = fd->path + strlen(node->path) - 1;
    if(!node->remove)
        return ENOSYS;

    std::int32_t status = node->remove(fd,fs_love_name);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
}

std::int32_t vfs::vfs::ls(userspace_fd_t* fd, dirent_t* out) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return ENOENT;

    char* fs_love_name = fd->path + strlen(node->path) - 1;
    if(!node->ls)
        return ENOSYS;

    std::int32_t status = node->ls(fd,fs_love_name,out);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
} 

std::int32_t vfs::vfs::var(userspace_fd_t* fd, std::uint64_t value, std::uint8_t request) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return ENOENT;

    char* fs_love_name = fd->path + strlen(node->path) - 1;
    if(!node->var)
        return ENOSYS;

    std::int32_t status = node->var(fd,fs_love_name,value,request);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
}

std::int32_t vfs::vfs::touch(char* path) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(path);
    if(!node)
        return ENOENT;

    char* fs_love_name = path + strlen(node->path) - 1;
    if(!node->touch)
        return ENOSYS;

    std::int32_t status = node->touch(fs_love_name);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
}

std::int32_t vfs::vfs::stat(userspace_fd_t* fd, stat_t* out) {
    vfs_lock->lock();
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return ENOENT;

    char* fs_love_name = fd->path + strlen(node->path) - 1;
    if(!node->stat)
        return ENOSYS;

    std::int32_t status = node->stat(fd,fs_love_name,out);
    if(!(status & (1 << 31)))
        vfs_lock->unlock();
    else if(status & (1 << 31))
        status &= ~(1 << 31);
    return status;
} 

void vfs::vfs::init() {
    memset(vfs_nodes,0,sizeof(vfs_nodes));

    vfs_lock = new locks::spinlock;
    
    tmpfs::mount(&vfs_nodes[0]);
    memcpy(vfs_nodes[0].path,"/",1);
    Log::Display(LEVEL_MESSAGE_OK,"TmpFS initializied\n");

    devfs::mount(&vfs_nodes[1]);
    memcpy(vfs_nodes[1].path,"/dev/",5);
    Log::Display(LEVEL_MESSAGE_OK,"DevFS initializied\n");
}