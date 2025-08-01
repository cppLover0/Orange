
#include <generic/vfs/vfs.hpp>
#include <cstdint>

#include <generic/vfs/devfs.hpp>

#include <generic/mm/pmm.hpp>

#include <etc/libc.hpp>
#include <etc/logging.hpp>

#include <etc/errno.hpp>

vfs::devfs_node_t* __devfs_head;

int dev_num = 0;
int is_slave = 0;

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
        if (!strcmp(temp, dev->slavepath) && dev_num == dev->dev_num) {
            is_slave = 1;
            return dev;
        } else if(!strcmp(temp,dev->masterpath) && dev_num == dev->dev_num) {
            is_slave = 0;
            return dev;
        }
        dev = dev->next;
    }

    return 0;
}

std::int32_t __devfs__open(userspace_fd_t* fd, char* path) {
    vfs::devfs_node_t* node = devfs_find_dev(path);
    if(node->open_flags.is_pipe) {
        fd->pipe = (vfs::pipe*)(node->pipe0 | (((uint64_t)node->pipe0 & (1 << 62)) << 63));
        fd->state = USERSPACE_FD_STATE_PIPE;
        fd->pipe_side = (node->pipe0 & (1 << 63)) == 1 ? PIPE_SIDE_WRITE : PIPE_SIDE_READ;
    } 
    fd->other_state = USERSPACE_FD_OTHERSTATE_SLAVE; 
    return 0;    
}

std::int64_t __devfs__write(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size) {
    vfs::devfs_packet_t packet;
    packet.request = fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? DEVFS_PACKET_WRITE_READRING : DEVFS_PACKET_WRITE_WRITERING;
    packet.cycle = &fd->cycle;
    packet.queue = &fd->queue;
    packet.size = size;
    packet.value = (std::uint64_t)buffer;
    return vfs::devfs::send_packet(path,&packet);
}

std::int64_t __devfs__read(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count) {
    vfs::devfs_packet_t packet;
    packet.request = fd->other_state == USERSPACE_FD_OTHERSTATE_MASTER ? DEVFS_PACKET_READ_WRITERING : DEVFS_PACKET_READ_READRING;
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

extern locks::spinlock* vfs_lock;

std::int64_t vfs::devfs::send_packet(char* path,devfs_packet_t* packet) {
    devfs_node_t* node = devfs_find_dev(path);
    switch(packet->request) {
        case DEVFS_PACKET_CREATE_DEV: {
            devfs_node_t* new_node = (devfs_node_t*)memory::pmm::_virtual::alloc(4096);
            memcpy(new_node->slavepath,path,strlen(path)); 
            memcpy(new_node->masterpath,(char*)packet->value,strlen((const char*)packet->value)); /* Actually path is slavepath and pcket.value is masterpath when driver creates new device */
            new_node->readring = new Lists::Ring(128);
            new_node->writering = new Lists::Ring(128);
            new_node->dev_num = packet->size;
            new_node->next = __devfs_head;
            __devfs_head = new_node;
            return 0;
        }

        case DEVFS_PACKET_CREATE_PIPE_DEV: {
            devfs_node_t* new_node = (devfs_node_t*)memory::pmm::_virtual::alloc(4096);
            memcpy(new_node->slavepath,path,strlen(path)); 
            memcpy(new_node->masterpath,(char*)packet->value,strlen((const char*)packet->value));
            new_node->readpipe = new pipe(0);
            new_node->writepipe = new pipe(0);
            new_node->readpipe->create(PIPE_SIDE_READ);
            new_node->readpipe->create(PIPE_SIDE_WRITE);
            new_node->writepipe->create(PIPE_SIDE_READ);
            new_node->writepipe->create(PIPE_SIDE_WRITE);
            new_node->dev_num = packet->size;
            new_node->open_flags.is_pipe_rw = 1;
            new_node->next = __devfs_head;
            __devfs_head = new_node;
            return 0;
        }

        case DEVFS_PACKET_READ_READRING: {
            if(!node->open_flags.is_pipe_rw) {
                std::int32_t count = node->readring->receivevals((std::uint64_t*)packet->value,dev_num,packet->size,packet->cycle,packet->queue);
                return count;
            } else {
                vfs_lock->unlock();
                std::int32_t count = node->readpipe->read((char*)packet->value,packet->size);
                return count | (1 << 63);
            }
        }

        case DEVFS_PACKET_WRITE_READRING: 
            if(!node->open_flags.is_pipe_rw)
                node->readring->send(dev_num,packet->value);
            else 
                return node->readpipe->write((char*)packet->value,packet->size);
            return 8;

        case DEVFS_PACKET_READ_WRITERING: {
            if(!node->open_flags.is_pipe_rw) {
                std::int32_t count = node->writering->receivevals((std::uint64_t*)packet->value,dev_num,packet->size,packet->cycle,packet->queue);
                return count;
            } else {
                vfs_lock->unlock();
                std::int32_t count = node->writepipe->read((char*)packet->value,packet->size);
                return count | (1 << 63);
            }
        }

        case DEVFS_PACKET_WRITE_WRITERING:
            if(!node->open_flags.is_pipe_rw)
                node->writering->send(dev_num,packet->value);
            else 
                return node->writepipe->write((char*)packet->value,packet->size);
            return 8;

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
            node->pipe0 = packet->enable_pipe.pipe_pointer;
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