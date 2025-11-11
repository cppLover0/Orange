
#include <generic/vfs/vfs.hpp>
#include <cstdint>

#include <generic/vfs/devfs.hpp>

#include <generic/mm/pmm.hpp>

#include <etc/libc.hpp>
#include <etc/logging.hpp>

#include <generic/mm/paging.hpp>

#include <drivers/ioapic.hpp>
#include <arch/x86_64/interrupts/irq.hpp>

#include <etc/errno.hpp>

vfs::devfs_node_t* __devfs_head;

int dev_num = 0;
int is_slave = 0;

bool isdigit(char ch) {
    return ch >= '0' && ch <= '9';
}

long reverse_number(long num) {
    long reversed_num = 0;
    
    while(num > 0) {
        int digit = num % 10;     
        reversed_num = reversed_num * 10 + digit; 
        num /= 10;                
    }
    
    return reversed_num;
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

    dev_num = reverse_number(dev_num);

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
        fd->pipe_side = (node->pipe0 & (1 << 63)) ? PIPE_SIDE_WRITE : PIPE_SIDE_READ;
    } else {
        fd->queue = is_slave == 1 ? node->readring->ring.tail : node->writering->ring.tail;
        fd->cycle = is_slave == 1 ? node->readring->ring.cycle : node->writering->ring.cycle;
    }

    fd->is_a_tty = node->is_tty;
    fd->other_state = is_slave == 1 ? USERSPACE_FD_OTHERSTATE_SLAVE : USERSPACE_FD_OTHERSTATE_MASTER;
    return 0;    
}

int is_printable(char c) {
    return (c >= 32 && c <= 126) || c == 10; 
}

std::int64_t __devfs__write(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size) {

    vfs::devfs_node_t* node = devfs_find_dev(path);

    if(!node) { vfs::vfs::unlock();
        return -ENOENT; }

    if(node->write) 
        return node->write(fd,buffer,size);

    if(is_slave == 1 && node->slave_write)
        return node->slave_write(fd,buffer,size);

    if(node->is_tty && !is_slave) { // handle stdio logic from /dev/pty writing 
        vfs::vfs::unlock();
        for(int i = 0;i < size;i++) {
            char c = ((char*)buffer)[i];

            if(!(node->term_flags->c_lflag & ICANON)) {
                if(c == '\n') c = 13;
            }

            if((node->term_flags->c_lflag & ECHO) && (is_printable(c) || c == 13)) {
                if(c == 13) {
                    node->writepipe->write("\n",1);
                } else if(c == '\n') 
                    node->writepipe->write("\r",1);
                asm volatile("cli");
                node->writepipe->write(&((char*)buffer)[i],1);
                asm volatile("cli"); 
            }

            if(c == '\b') {
                if(node->readpipe->size > 0) {
                    const char* back = "\b \b";
                    node->writepipe->write(back,strlen(back));
                    asm volatile("cli");
                    node->readpipe->lock.lock();
                    node->readpipe->read_counter--;
                    node->readpipe->buffer[node->readpipe->size--] = '\0';
                    node->readpipe->lock.unlock(); 
                }
            }

            if(is_printable(c) || !(node->term_flags->c_lflag & ICANON)) {
                node->readpipe->write(&c,1);
                asm volatile("cli");
            } else if(c == 13) {
                node->readpipe->write("\n",1);
                asm volatile("cli");
            }

            if((c == '\n' || c == 13) && (node->term_flags->c_lflag & ICANON)) {
                node->readpipe->set_tty_ret(); 
            }
            asm volatile("cli");
        }
        return size;
    } else if(node->is_tty && is_slave) {
        vfs::vfs::unlock();
        for(int i = 0;i < size;i++) {
            char c = ((char*)buffer)[i]; 
            if(c == '\n') {
                node->writepipe->write("\r\n",2);
            } else
                node->writepipe->write(&c,1);
            asm volatile("cli");
        }
        return size;
    }

    std::uint64_t status;
    int is_master = !is_slave;
    if(node->open_flags.is_pipe_rw) {
        vfs::vfs::unlock();
        status = is_master ? node->readpipe->write((char*)buffer,size) : node->writepipe->write((char*)buffer,size);
    } else {
        if(!is_master) {
            if(node->writering->ring.bytelen == 1) {
                    node->writering->send(*(char*)buffer);
            } else if(node->writering->ring.bytelen == 2) {
                    node->writering->send(*(short*)buffer);
            } else if(node->writering->ring.bytelen == 4) {
                    node->writering->send(*(int*)buffer);
            } else if(node->writering->ring.bytelen == 8) {
                    node->writering->send(*(long long*)buffer);
            }
        } else {
            if(node->readring->ring.bytelen == 1) {
                    node->readring->send(*(char*)buffer);
            } else if(node->readring->ring.bytelen == 2) {
                    node->readring->send(*(short*)buffer);
            } else if(node->readring->ring.bytelen == 4) {
                    node->readring->send(*(int*)buffer);
            } else if(node->readring->ring.bytelen == 8) {
                    node->readring->send(*(long long*)buffer);
            }
        }
        vfs::vfs::unlock();
        return node->writering->ring.bytelen;
    }

    return status;
}

std::int64_t __devfs__read(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count) {

    vfs::devfs_node_t* node = devfs_find_dev(path);

    if(!node) { vfs::vfs::unlock();
        return -ENOENT; }

    if(node->read)
        return node->read(fd,buffer,count);

    std::uint64_t status;
    int is_master = !is_slave;

    if(node->open_flags.is_pipe_rw) {
        vfs::vfs::unlock();
        status = is_master ? node->writepipe->read(&fd->read_counter,(char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0) : node->readpipe->read(&fd->read_counter,(char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0);
    } else {
        //DEBUG(1,"read %s ring count %d",path,count);
        status = is_master ? node->writering->receivevals(buffer,dev_num,count,&fd->cycle,&fd->queue) : node->readring->receivevals(buffer,dev_num,count,&fd->cycle,&fd->queue);
        vfs::vfs::unlock();
    }

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

    vfs::devfs_node_t* node = devfs_find_dev(path);
    if(!node)
        return ENOENT;

    int status = 0;
    if(request == DEVFS_VAR_ISATTY)
        status = node->is_tty ? 0 : ENOTTY;
    
    if((request & ~(1 << 7)) == TMPFS_VAR_CHMOD) {
        if(request & (1 << 7))
            node->mode = value & 0xFFFFFFFF;
        else 
            *(std::uint64_t*)value = (std::uint64_t)node->mode;
    }

    return status;
}

std::int32_t __devfs__stat(userspace_fd_t* fd, char* path, vfs::stat_t* out) {
    if(!path)
        return EFAULT;

    vfs::devfs_node_t* node = devfs_find_dev(path);
    
    if(!node)
        return ENOENT;

    if(!out)
        return EINVAL;

    out->st_size = 0;
    out->st_mode = S_IFCHR;
    return 0;
}

std::int64_t __devfs__poll(userspace_fd_t* fd, char* path, int operation_type) {

    vfs::devfs_node_t* node = devfs_find_dev(path);

    std::int64_t ret = 0;
    if(node->open_flags.is_pipe_rw) {
        switch (operation_type)
        {
            
        case POLLIN:
            ret = !is_slave ? node->writepipe->read_counter : node->readpipe->read_counter;
            if(!is_slave)
                if(fd->read_counter == -1 && node->writepipe->size != 0)
                    ret = 0;
            else 
                if(fd->read_counter == -1 && node->readpipe->size != 0)
                    ret = 0;

            if(!is_slave) {
                if(fd->read_counter < node->writepipe->read_counter && node->writepipe->size == 0) {
                    fd->read_counter = node->writepipe->read_counter;
                } else if(fd->read_counter < node->readpipe->read_counter && node->readpipe->size == 0)
                    fd->read_counter = node->readpipe->read_counter;
            }

            break;
            
        case POLLOUT:
            ret = !is_slave ? node->readpipe->write_counter : node->writepipe->write_counter;
            if(ret == fd->write_counter) {
                if(!is_slave) {
                    if(node->readpipe->size != node->readpipe->total_size) {
                        // fix this shit
                        node->readpipe->write_counter++;
                    }
                } else {
                    if(node->writepipe->size != node->writepipe->total_size) {
                        node->writepipe->write_counter++;
                    }
                }
                ret = !is_slave ? node->readpipe->write_counter : node->writepipe->write_counter;
            }
            
            break;
            
        default:
            break;
        }
    } else {
        if(operation_type == POLLIN) {
            ret = !is_slave ? node->writering->read_counter : node->readring->read_counter;
            if(!is_slave && ret == fd->read_counter) {
                if(node->writering->isnotempty((int*)&fd->queue,(char*)&fd->cycle)) {
                    fd->read_counter--;
                } 
            } else if(ret == fd->read_counter && is_slave) {
                if(node->readring->isnotempty((int*)&fd->queue,(char*)&fd->cycle)) {    
                    fd->read_counter--;

                } 
            }

        } else if(operation_type == POLLOUT) {
            ret = !is_slave ? ++node->readring->write_counter : ++node->writering->write_counter;
        } 
    }
    return ret;
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

        case DEVFS_PACKET_IOCTL: {

            if(!node)
                return ENOENT;
                
            for(int i = 0;i < 32;i++) {
                if(node->ioctls[i].read_req == packet->ioctl.ioctlreq) { /* Read */
                    memcpy((void*)packet->ioctl.arg,node->ioctls[i].pointer_to_struct,node->ioctls[i].size);
                    return 0;
                } else if(node->ioctls[i].write_req == packet->ioctl.ioctlreq) { /* Write */
                    if(node->is_tty) {
                        if(packet->ioctl.ioctlreq == TCSETS) {
                            termios_t* termios = (termios_t*)packet->ioctl.arg;
                        }
                    }
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

                    if(current->write_req == TCSETS) {
                        node->writepipe->ttyflags = 0;
                        node->readpipe->ttyflags = (termios_t*)packet->create_ioctl.pointer;
                        node->term_flags = (termios_t*)packet->create_ioctl.pointer;
                    }

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

    memset(fd->path,0,2048);
    __printfbuf(fd->path,2048,"/dev/pty%d",tty_ptr);
    memset(ttyname,0,2048);
    __printfbuf(ttyname,2048,"/dev/tty%d",tty_ptr);

    packet.request = DEVFS_PACKET_SETUPTTY;
    packet.value = (std::uint64_t)fd->path + 4;

    vfs::devfs::send_packet(ttyname + 4,&packet);

    termios_t* t = (termios_t*)memory::pmm::_virtual::alloc(4096);
    
    t->c_lflag |= (ICANON | ECHO);

    t->c_iflag = IGNPAR | ICRNL;
    t->c_oflag = OPOST;         
    t->c_cflag |= (CS8); 

    packet.request = DEVFS_PACKET_CREATE_IOCTL;
    packet.create_ioctl.pointer = (std::uint64_t)t;
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

std::int64_t __irq_write(userspace_fd_t* fd, void* buffer, std::uint64_t size) {
    drivers::ioapic::unmask(dev_num);
    vfs::vfs::unlock();
    return size;
}

std::int64_t __pic_write(userspace_fd_t* fd, void* buffer, std::uint64_t size) {
    if(size != 10) {vfs::vfs::unlock();
        return -EINVAL; }

    std::uint16_t irq_num = *(std::uint16_t*)((std::uint64_t)buffer);
    std::uint64_t ioapic_flags = *(std::uint64_t*)((std::uint64_t)buffer + 2);

    const char* path0 = "/masterirq";
    const char* path1 = "/irq";

    vfs::devfs_node_t* new_node = (vfs::devfs_node_t*)memory::pmm::_virtual::alloc(4096);
    memcpy(new_node->slavepath,path1,strlen(path1)); 
    memcpy(new_node->masterpath,(char*)path0,strlen((const char*)path0)); 
    new_node->readring = new Lists::Ring(128);
    new_node->writering = new Lists::Ring(128);
    new_node->dev_num = irq_num;
    new_node->next = __devfs_head;
    new_node->slave_write = __irq_write;
    __devfs_head = new_node;

    new_node->writering->setup_bytelen(1);
    new_node->readring->setup_bytelen(1);

    arch::x86_64::interrupts::irq::create(irq_num,IRQ_TYPE_LEGACY_USERSPACE,0,0,ioapic_flags);   
    drivers::ioapic::unmask(irq_num); 
    
    vfs::vfs::unlock();
    return 10;

}

std::int64_t __zero_write(userspace_fd_t* fd, void* buffer, std::uint64_t size) {
    vfs::vfs::unlock();
    return size;
}

std::int64_t __zero_read(userspace_fd_t* fd, void* buffer, std::uint64_t count) {
    vfs::vfs::unlock(); // dont waste time on this
    memset(buffer,0,count); 
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
    node->stat = __devfs__stat;
    node->poll = __devfs__poll;

    const char* name = "/ptmx";

    devfs_packet_t packet;
    packet.size = tty_ptr;
    packet.request = DEVFS_PACKET_CREATE_PIPE_DEV;
    packet.value = (std::uint64_t)name;

    send_packet((char*)name,&packet);

    __devfs_head->open = __ptmx_open;

    const char* name0 = "/pic";

    packet.size = tty_ptr;
    packet.request = DEVFS_PACKET_CREATE_DEV;
    packet.value = (std::uint64_t)name0;

    send_packet((char*)name0,&packet);

    /* The head should be this dev so all ok */
    
    __devfs_head->write = __pic_write;


    const char* name00 = "/null";

    packet.size = 0;
    packet.request = DEVFS_PACKET_CREATE_DEV;
    packet.value = (std::uint64_t)name00;

    send_packet((char*)name00,&packet);
    
    __devfs_head->write = __zero_write;
    __devfs_head->read = __zero_read;

}