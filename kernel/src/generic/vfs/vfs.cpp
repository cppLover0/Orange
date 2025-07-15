
#include <generic/mm/pmm.hpp>
#include <generic/vfs/vfs.hpp>
#include <generic/vfs/tmpfs.hpp>

#include <cstdint>
#include <etc/errno.hpp>

#include <etc/logging.hpp>

vfs::vfs_node_t vfs_nodes[512];

vfs::vfs_node_t* find_node(char* path) {
    for(int i = 0;i < 512;i++) {
        if(!strncmp(path,vfs_nodes[i].path,strlen(vfs_nodes[i].path))) {
            return &vfs_nodes[i];
        }
    }
    return 0;
}

std::int64_t vfs::vfs::write(userspace_fd_t* fd, void* buffer, std::uint64_t size) {
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return -ENOENT;

    char* fs_love_name = &fd->path[0] + strlen(node->path) - 1;
    if(!node->write)
        return -ENOSYS;

    return node->write(fd,fs_love_name,buffer,size);
}

std::int64_t vfs::vfs::read(userspace_fd_t* fd, void* buffer, std::uint64_t count) {
    vfs_node_t* node = find_node(fd->path);
    if(!node)
        return -ENOENT;

    char* fs_love_name = &fd->path[0] + strlen(node->path) - 1;
    if(!node->read)
        return -ENOSYS;

    return node->read(fd,fs_love_name,buffer,count);
}

std::int32_t vfs::vfs::create(char* path, std::uint8_t type) {
    vfs_node_t* node = find_node(path);
    if(!node)
        return -ENOENT;

    char* fs_love_name = path + strlen(node->path) - 1;
    if(!node->read)
        return -ENOSYS;

    return node->create(path,type);
}

void vfs::vfs::init() {
    memset(vfs_nodes,0,sizeof(vfs_nodes));
    
    tmpfs::mount(&vfs_nodes[0]);
    memcpy(vfs_nodes[0].path,"/",1);

    Log::Display(LEVEL_MESSAGE_OK,"TmpFS initializied\n");
}