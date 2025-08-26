
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
    
    if(!node)
        return ENOENT;

    if(node->open)
        return node->open(fd,path);

    if(node->open_flags.is_pipe) {
        fd->pipe = (vfs::pipe*)(node->pipe0 | (((uint64_t)node->pipe0 & (1 << 62)) << 63));
        fd->state = USERSPACE_FD_STATE_PIPE;
        fd->pipe_side = (node->pipe0 & (1 << 63)) == 1 ? PIPE_SIDE_WRITE : PIPE_SIDE_READ;
    } else {
        fd->queue = is_slave == 1 ? node->readring->ring.tail : node->writering->ring.tail;
        fd->cycle = is_slave == 1 ? node->readring->ring.cycle : node->writering->ring.cycle;
    }
    fd->other_state = is_slave == 1 ? USERSPACE_FD_OTHERSTATE_SLAVE : USERSPACE_FD_OTHERSTATE_MASTER;
    return 0;    
}

std::int64_t __devfs__write(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size) {

    vfs::devfs_node_t* node = devfs_find_dev(path);

    if(!node) { vfs::vfs::unlock();
        return -ENOENT; }

    if(node->write) 
        return node->write(fd,buffer,size);

    vfs::devfs_packet_t packet;
    packet.request = !is_slave ? DEVFS_PACKET_WRITE_READRING : DEVFS_PACKET_WRITE_WRITERING;
    packet.cycle = &fd->cycle;
    packet.queue = &fd->queue;
    packet.size = size;
    packet.value = (std::uint64_t)buffer;

    return vfs::devfs::send_packet(path,&packet);
}

std::int64_t __devfs__read(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count) {

    vfs::devfs_node_t* node = devfs_find_dev(path);

    if(!node) { vfs::vfs::unlock();
        return -ENOENT; }

    if(node->read)
        return node->read(fd,buffer,count);

    vfs::devfs_packet_t packet;
    packet.request = !is_slave ? DEVFS_PACKET_READ_WRITERING : DEVFS_PACKET_READ_READRING;
    packet.cycle = &fd->cycle;
    packet.queue = &fd->queue;
    packet.size = count;
    packet.value = (std::uint64_t)buffer;

    std::int64_t status = vfs::devfs::send_packet(path,&packet);

    return status;
}

std::int32_t __devfs__ioctl(userspace_fd_t* fd, char* path, unsigned long req, void *arg, int *res) {

    vfs::devfs_node_t* node = devfs_find_dev(path);
    if(!node)
        return ENOENT;

    if(node->ioctl)
        return node->ioctl(fd,req,arg,res);


    /* Kernel requests size */
    if(arg == 0) {
        vfs::devfs_packet_t packet;
        packet.request = DEVFS_PACKET_SIZE_IOCTL;
        packet.ioctl.ioctlreq = req;
        return vfs::devfs::send_packet(path,&packet);
    }

    vfs::devfs_packet_t packet;
    packet.request = DEVFS_PACKET_IOCTL;
    packet.ioctl.arg = (std::uint64_t)arg;
    packet.ioctl.ioctlreq = req;
    std::int64_t status = vfs::devfs::send_packet(path,&packet);
    *res = status;
    return status;
}

std::int32_t __devfs__mmap(userspace_fd_t* fd, char* path, std::uint64_t* outp, std::uint64_t* outsz, std::uint64_t* outflags) {
    vfs::devfs_node_t* node = devfs_find_dev(path);
    if(!node)
        return ENOENT;

    *outp = node->mmap_base;
    *outsz = node->mmap_size;
    *outflags = node->mmap_flags;

    return 0;
}

std::int32_t __devfs__var(userspace_fd_t* fd, char* path, std::uint64_t value, std::uint8_t request) {

    vfs::devfs_packet_t packet;
    packet.request = DEVFS_PACKET_ISATTY;
    std::int32_t status = vfs::devfs::send_packet(path,&packet);
    return status;
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

            if(!node) { vfs::vfs::unlock();
                return -ENOENT; }

            if(!node->open_flags.is_pipe_rw) {
                
                std::uint64_t count = node->readring->receivevals((std::uint64_t*)packet->value,dev_num,packet->size,packet->cycle,packet->queue);
                vfs::unlock();
                return count;
            } else {
                vfs::unlock();
                std::uint64_t count = node->readpipe->read((char*)packet->value,packet->size);
                return count;
            }
        }

        case DEVFS_PACKET_WRITE_READRING: 

            if(!node) { vfs::vfs::unlock();
                return -ENOENT; }
                
            if(!node->open_flags.is_pipe_rw) {
                if(node->readring->ring.bytelen == 1) {
                    node->readring->send(*(char*)packet->value);
                } else if(node->readring->ring.bytelen == 2) {
                    node->readring->send(*(short*)packet->value);
                } else if(node->readring->ring.bytelen == 4) {
                    node->readring->send(*(int*)packet->value);
                } else if(node->readring->ring.bytelen == 8) {
                    node->readring->send(*(long long*)packet->value);
                }
                vfs::vfs::unlock();
            } else { 
                vfs::vfs::unlock();
                return node->readpipe->write((char*)packet->value,packet->size);
            }

            return 8;

        case DEVFS_PACKET_READ_WRITERING: {

            if(!node) { vfs::vfs::unlock();
                return -ENOENT; }
                
            if(!node->open_flags.is_pipe_rw) {
                std::int32_t count = node->writering->receivevals((std::uint64_t*)packet->value,dev_num,packet->size,packet->cycle,packet->queue);
                vfs::unlock();
                return count;
            } else {
                vfs::unlock();
                std::int64_t count = node->writepipe->read((char*)packet->value,packet->size);
                return count;
            }
        }

        case DEVFS_PACKET_WRITE_WRITERING:

            if(!node) { vfs::vfs::unlock();
                return -ENOENT; }
                
            if(!node->open_flags.is_pipe_rw) {
                if(node->writering->ring.bytelen == 1) {
                    node->writering->send(*(char*)packet->value);
                } else if(node->writering->ring.bytelen == 2) {
                    node->writering->send(*(short*)packet->value);
                } else if(node->writering->ring.bytelen == 4) {
                    node->writering->send(*(int*)packet->value);
                } else if(node->writering->ring.bytelen == 8) {
                    node->writering->send(*(long long*)packet->value);
                }
                vfs::vfs::unlock();
            } else {
                vfs::vfs::unlock();
                return node->writepipe->write((char*)packet->value,packet->size);
            }
            return 8;

        case DEVFS_PACKET_IOCTL: {

            if(!node)
                return ENOENT;
                
            for(int i = 0;i < 32;i++) {
                if(node->ioctls[i].read_req == packet->ioctl.ioctlreq) { /* Read */
                    memcpy((void*)packet->ioctl.arg,node->ioctls[i].pointer_to_struct,node->ioctls[i].size);
                    return 0;
                } else if(node->ioctls[i].write_req == packet->ioctl.ioctlreq) { /* Write */
                    memcpy(node->ioctls[i].pointer_to_struct,(void*)packet->ioctl.arg,node->ioctls[i].size);
                    return 0;
                }
            }
            return EINVAL;
        }
        case DEVFS_PACKET_SIZE_IOCTL: {

            if(!node)
                return -ENOENT;
                
            for(int i = 0;i < 32;i++) {
                if(node->ioctls[i].read_req == packet->ioctl.ioctlreq) { /* Read */
                    return node->ioctls[i].size;
                } else if(node->ioctls[i].write_req == packet->ioctl.ioctlreq) { /* Write */
                    return node->ioctls[i].size;
                }
            }
            return -EINVAL;
        }
        case DEVFS_PACKET_CREATE_IOCTL: {

            if(!node)
                return ENOENT;

            for(int i = 0;i < 32;i++) {
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

            if(!node)
                return ENOENT;
                
            node->pipe0 = packet->enable_pipe.pipe_pointer;
            node->open_flags.is_pipe = 1;
            return 0;
        }

        case DEVFS_SETUP_MMAP: {

            if(!node)
                return ENOENT;
                
            node->mmap_base = packet->setup_mmap.dma_addr;
            node->mmap_size = packet->setup_mmap.size;
            node->mmap_flags = packet->setup_mmap.flags;
            return 0;
        }

        case DEVFS_PACKET_ISATTY:

            if(!node)
                return ENOENT;
                
            if(node->is_tty)
                return 0;
            else
                return ENOTTY;

        case DEVFS_PACKET_SETUPTTY:
            
            if(!node)
                return ENOENT;

            node->is_tty = 1;
            return 0;

        case DEVFS_GETSLAVE_BY_MASTER:

            if(!node)
                return ENOENT;

            __printfbuf((char*)packet->value,256,"%s%d",node->slavepath,node->dev_num);

            return 0;

        case DEVFS_PACKET_SETUP_RING_SIZE:
            
            if(!node)
                return ENOENT;

            if(!node->open_flags.is_pipe) {
                node->readring->setup_bytelen(packet->size);
                node->writering->setup_bytelen(packet->size);
            }

            return 0;

    }

    vfs::vfs::unlock();
    return 0;
}

int tty_ptr = 0;
std::int32_t __ptmx_open(userspace_fd_t* fd, char* path) {

    memset(fd->path,0,2048);
    __printfbuf(fd->path,2048,"/dev/pty");
    char ttyname[2048];
    memset(ttyname,0,2048);
    __printfbuf(ttyname,2048,"/dev/tty");

    vfs::devfs_packet_t packet;
    packet.size = tty_ptr;
    packet.request = DEVFS_PACKET_CREATE_PIPE_DEV;
    packet.value = (std::uint64_t)fd->path + 4;

    vfs::devfs::send_packet(ttyname + 4,&packet);

    packet.request = DEVFS_PACKET_SETUPTTY;
    packet.value = (std::uint64_t)fd->path + 4;

    vfs::devfs::send_packet(ttyname + 4,&packet);

    packet.request = DEVFS_PACKET_CREATE_IOCTL;
    packet.create_ioctl.pointer = (uint64_t)memory::pmm::_virtual::alloc(4096);
    packet.create_ioctl.size = sizeof(termios_t);
    packet.create_ioctl.readreg = TCGETS;
    packet.create_ioctl.writereg = TCSETS;

    vfs::devfs::send_packet(ttyname + 4,&packet);

    packet.request = DEVFS_PACKET_CREATE_IOCTL;
    packet.create_ioctl.pointer = (uint64_t)memory::pmm::_virtual::alloc(4096);
    packet.create_ioctl.size = sizeof(struct winsize);
    packet.create_ioctl.readreg = TIOCGWINSZ;
    packet.create_ioctl.writereg = TIOCSWINSZ;

    vfs::devfs::send_packet(ttyname + 4,&packet);

    fd->other_state = USERSPACE_FD_OTHERSTATE_MASTER;
    __printfbuf(fd->path,2048,"/dev/pty%d",tty_ptr);

    tty_ptr++;

    return 0;
}

void vfs::devfs::mount(vfs_node_t* node) {
    __devfs_head = (devfs_node_t*)memory::pmm::_virtual::alloc(4096);
    node->open = __devfs__open;
    node->read = __devfs__read;
    node->write = __devfs__write;
    node->ioctl = __devfs__ioctl;
    node->mmap = __devfs__mmap;
    node->var = __devfs__var;

    const char* name = "/ptmx";

    devfs_packet_t packet;
    packet.size = tty_ptr;
    packet.request = DEVFS_PACKET_CREATE_PIPE_DEV;
    packet.value = (std::uint64_t)name;

    send_packet((char*)name,&packet);
    /* The head should be this dev so all ok */
    
    __devfs_head->open = __ptmx_open;

}