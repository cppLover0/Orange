
#include <generic/vfs/vfs.hpp>
#include <cstdint>

#include <generic/vfs/devfs.hpp>

#include <generic/mm/pmm.hpp>

#include <etc/libc.hpp>
#include <etc/logging.hpp>

#include <etc/errno.hpp>

vfs::devfs_node_t* __devfs_head;

int dev_num = 0;

bool isdigit(char ch) {
    return ch >= '0' && ch <= '9';
}

vfs::devfs_node_t* devfs_find_dev(const char* loc) {
    vfs::devfs_node_t* dev = __devfs_head;
    char temp[256]; 
    int len = strlen((char*)loc);
    int i = len - 1;

    dev_num = 0;

    while (i >= 0 && isdigit(loc[i])) {
        dev_num = dev_num * 10 + (loc[i] - '0'); 
        i--;
    }

    memcpy(temp, loc, i + 1);
    temp[i + 1] = '\0'; 

    while (dev) {
        if (!strcmp(temp, dev->path)) {
            return dev;
        }
        dev = dev->next;
    }

    return 0;
}

std::int32_t __devfs__open(userspace_fd_t* fd, char* path) {
    vfs::devfs_node_t* node = devfs_find_dev(path);
    if(node->open_flags.is_pipe) {
        if(dev_num >= 32)
            return EFAULT;
        fd->pipe = (vfs::pipe*)(node->pipes[dev_num] | ((uint64_t)node->pipes[dev_num] & (1 << 62)));
        fd->state = USERSPACE_FD_STATE_PIPE;
        fd->pipe_side = (node->pipes[dev_num] & (1 << 63)) == 1 ? PIPE_SIDE_WRITE : PIPE_SIDE_READ;
    }
    return 0;    
}

std::int64_t __devfs__write(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size) {
    vfs::devfs_packet_t packet;
    packet.request = DEVFS_PACKET_WRITE_WRITERING;
    packet.cycle = &fd->cycle;
    packet.queue = &fd->queue;
    packet.size = size;
    packet.value = (std::uint64_t)buffer;
    return vfs::devfs::send_packet(path,&packet);
}

std::int64_t __devfs__read(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count) {
    vfs::devfs_packet_t packet;
    packet.request = DEVFS_PACKET_READ_READRING;
    packet.cycle = &fd->cycle;
    packet.queue = &fd->queue;
    packet.size = count;
    packet.value = (std::uint64_t)buffer;
    return vfs::devfs::send_packet(path,&packet);
}

std::int32_t __devfs__ioctl(userspace_fd_t* fd, char* path, unsigned long req, void *arg, int *res) {
    vfs::devfs_packet_t packet;
    packet.request = DEVFS_PACKET_IOCTL;
    packet.ioctl.arg = (std::uint64_t)arg;
    packet.ioctl.ioctlreq = req;
    std::int32_t status = vfs::devfs::send_packet(path,&packet);
    *res = status;
    return status;
}

std::int32_t __devfs__mmap(userspace_fd_t* fd, char* path, std::uint64_t* outp, std::uint64_t* outsz) {
    vfs::devfs_node_t* node = devfs_find_dev(path);
    if(!node)
        return ENOENT;

    *outp = node->mmap_base;
    *outsz = node->mmap_size;

    return 0;
}

std::int32_t vfs::devfs::send_packet(char* path,devfs_packet_t* packet) {
    devfs_node_t* node = devfs_find_dev(path);
    if(dev_num >= 32)
        return -EFAULT;
    switch(packet->request) {
        case DEVFS_PACKET_CREATE_DEV: {
            devfs_node_t* new_node = (devfs_node_t*)memory::pmm::_virtual::alloc(4096);
            memcpy(new_node->path,path,strlen(path));
            new_node->readring = new Lists::Ring(128);
            new_node->writering = new Lists::Ring(128);
            return 0;
        }

        case DEVFS_PACKET_READ_READRING: 
            return node->readring->receivevals((std::uint64_t*)packet->value,dev_num,packet->size,packet->cycle,packet->queue);

        case DEVFS_PACKET_WRITE_READRING: 
            node->readring->send(dev_num,packet->value);
            return 0;

        case DEVFS_PACKET_READ_WRITERING:
            return node->writering->receivevals((std::uint64_t*)packet->value,dev_num,packet->size,packet->cycle,packet->queue);

        case DEVFS_PACKET_WRITE_WRITERING:
            node->writering->send(dev_num,packet->value);
            return 0;

        case DEVFS_PACKET_IOCTL: {
            for(int i = 0;i < 128;i++) {
                if(node->ioctls[i].read_req == packet->ioctl.ioctlreq) { /* Read */
                    memcpy((void*)packet->ioctl.arg,node->ioctls[i].pointer_to_struct,node->ioctls[i].size);
                    return 0;
                } else if(node->ioctls[i].write_req == packet->ioctl.ioctlreq) { /* Write */
                    memcpy(node->ioctls[i].pointer_to_struct,(void*)packet->ioctl.arg,node->ioctls[i].size);
                    return 0;
                }
            }
            return -EINVAL;
        }
        case DEVFS_PACKET_SIZE_IOCTL: {
            for(int i = 0;i < 128;i++) {
                if(node->ioctls[i].read_req == packet->ioctl.ioctlreq) { /* Read */
                    return node->ioctls[i].size;
                } else if(node->ioctls[i].write_req == packet->ioctl.ioctlreq) { /* Write */
                    return node->ioctls[i].size;
                }
            }
            return -EINVAL;
        }
        case DEVFS_PACKET_CREATE_IOCTL: {
            for(int i = 0;i < 128;i++) {
                if(node->ioctls[i].pointer_to_struct == 0) {
                    devfs_ioctl_packet_t* current = &node->ioctls[i];
                    current->read_req = packet->create_ioctl.readreg;
                    current->write_req = packet->create_ioctl.writereg;
                    current->pointer_to_struct = (void*)packet->create_ioctl.pointer;
                    current->size = packet->create_ioctl.size;
                    return 0;
                }
            }
        }
        case DEVFS_ENABLE_PIPE: {
            node->pipes[packet->enable_pipe.pipe_target] = packet->enable_pipe.pipe_pointer;
            node->open_flags.is_pipe = 1;
            return 0;
        }

        case DEVFS_SETUP_MMAP: {
            node->mmap_base = packet->setup_mmap.dma_addr;
            node->mmap_size = packet->setup_mmap.size;
            return 0;
        }

        default:
            return -EINVAL;
    }

    return 0;
}

void vfs::devfs::mount(vfs_node_t* node) {
    __devfs_head = (devfs_node_t*)memory::pmm::_virtual::alloc(4096);
    node->open = __devfs__open;
    node->read = __devfs__read;
    node->write = __devfs__write;
    node->ioctl = __devfs__ioctl;
    node->mmap = __devfs__mmap;
}