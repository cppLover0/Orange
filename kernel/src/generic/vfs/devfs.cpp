
#include <generic/vfs/vfs.hpp>
#include <cstdint>

#include <generic/vfs/devfs.hpp>

#include <generic/mm/pmm.hpp>

#include <etc/libc.hpp>
#include <etc/logging.hpp>

#include <generic/mm/paging.hpp>

#include <drivers/ioapic.hpp>
#include <arch/x86_64/interrupts/irq.hpp>

#include <etc/assembly.hpp>

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
                    node->writepipe->write("\n",1,0);
                } else if(c == '\n') 
                    node->writepipe->write("\r",1,0);
                asm volatile("cli");
                node->writepipe->write(&((char*)buffer)[i],1,0);
                asm volatile("cli"); 
            }

            if(c == '\b' || c == '\x7f' || c == '\x08') {
                if(node->readpipe->size > 0) {
                    if(node->term_flags->c_lflag & ECHO) {
                        const char* back = "\b \b";
                        node->writepipe->write(back,strlen(back),0);
                        asm volatile("cli");
                    }
                    node->readpipe->lock.lock();
                    node->readpipe->read_counter--;
                    node->readpipe->buffer[node->readpipe->size--] = '\0';
                    node->readpipe->lock.unlock(); 
                }
            }

            if(is_printable(c) || !(node->term_flags->c_lflag & ICANON)) {
                node->readpipe->write(&c,1,0);
                asm volatile("cli");
            } else if(c == 13) {
                node->readpipe->write("\n",1,0);
                asm volatile("cli");
            }

            if((c == '\n' || c == 13) && (node->term_flags->c_lflag & ICANON)) {
                node->readpipe->set_tty_ret(); 
            }

            if(!(node->term_flags->c_lflag & ICANON) && node->readpipe->size >= node->term_flags->c_cc[VMIN]) {
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
                node->writepipe->write("\r\n",2,0);
            } else
                node->writepipe->write(&c,1,0);
            asm volatile("cli");
        }
        return size;
    }

    std::uint64_t status;
    int is_master = !is_slave;
    if(node->open_flags.is_pipe_rw) {
        vfs::vfs::unlock();
        status = is_master ? node->readpipe->write((char*)buffer,size,0) : node->writepipe->write((char*)buffer,size,0);
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
        if(!node->is_tty)
            status = is_master ? node->writepipe->read(&fd->read_counter,(char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0) : node->readpipe->read(&fd->read_counter,(char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0);
        else 
            status = is_master ? node->writepipe->read(&fd->read_counter,(char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0) : node->readpipe->ttyread(&fd->read_counter,(char*)buffer,count,(fd->flags & O_NONBLOCK) ? 1 : 0);
    } else {
        //DEBUG(1,"read %s ring count %d",path,count);
        status = is_master ? node->writering->receivevals(buffer,dev_num,count,&fd->cycle,&fd->queue) : node->readring->receivevals(buffer,dev_num,count,&fd->cycle,&fd->queue);
        vfs::vfs::unlock();
    }

    return status;
}

#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410


std::int32_t __devfs__ioctl(userspace_fd_t* fd, char* path, unsigned long req, void *arg, int *res) {

    vfs::devfs_node_t* node = devfs_find_dev(path);
    if(!node)
        return ENOENT;

    if(node->ioctl)
        return node->ioctl(fd,req,arg,res);

    if(!arg)
        return 0;

    if(req == 0x80045430 && node->is_tty) {
        // get pty num
        vfs::devfs_packet_t packet;
        packet.request = devfs_packet_get_num;
        packet.value = 0;
        std::int64_t status = vfs::devfs::send_packet(path,&packet);
        *res = status;
        *((int*)arg) = packet.value;
        return status;
    } else if(req == TIOCGPGRP && node->is_tty) {
        *(int*)arg = node->pgrp;
        *res = 0;
        return 0;
    } else if(req == TIOCSPGRP && node->is_tty) {
        node->pgrp = *(int*)arg;
        *res = 0;
        return 0;
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
    out->st_ino = 4;
    out->st_mode |= 020666;
    return 0;
}

std::int64_t __devfs__poll(userspace_fd_t* fd, char* path, int operation_type) {

    vfs::devfs_node_t* node = devfs_find_dev(path);

    std::int64_t ret = 0;
    if(node->open_flags.is_pipe_rw) {
        node->writepipe->lock.lock();
        node->readpipe->lock.lock();
        if(operation_type == POLLIN) {
            if(!is_slave) {
                ret = node->writepipe->size != 0 ? 1 : 0;
            } else {
                if(!node->is_tty)
                    ret = node->readpipe->size != 0 ? 1 : 0;
                else {
                    ret = (node->readpipe->tty_ret && node->readpipe->size != 0) != 0 ? 1 : 0;
                }
            }
        } else if(operation_type == POLLOUT) {
            if(!is_slave) {
                ret = node->writepipe->size != node->writepipe->total_size ? 1 : 0;
            } else {
                ret = node->readpipe->size != node->readpipe->total_size ? 1 : 0;
            }
        }
        node->writepipe->lock.unlock();
        node->readpipe->lock.unlock();
    } else {
        if(operation_type == POLLIN) {
            ret = 0;
            if(!is_slave) {
                if(node->writering->isnotempty((int*)&fd->queue,(char*)&fd->cycle)) {
                    ret = 1;
                } 
            } else if(is_slave) {
                if(node->readring->isnotempty((int*)&fd->queue,(char*)&fd->cycle)) {    
                    ret = 1;

                } 
            }

        } else if(operation_type == POLLOUT) {
            ret = 1;
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
            //DEBUG(1,"%d %d\n",new_node->readpipe->size.load(),new_node->writepipe->size.load());
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
                
            if(!packet->ioctl.arg)
                return 0; 

            std::uint64_t originalreq = packet->ioctl.ioctlreq;

            if(packet->ioctl.ioctlreq == TCGETS2)
                packet->ioctl.ioctlreq = TCGETS;
            
            if(packet->ioctl.ioctlreq == TCSETS2 || packet->ioctl.ioctlreq == TCSETSW2 || packet->ioctl.ioctlreq == TCSETSF2)
                packet->ioctl.ioctlreq = TCSETS;

            for(int i = 0;i < 32;i++) {
                if(node->ioctls[i].read_req == packet->ioctl.ioctlreq) { /* Read */
                    if(node->is_tty) {
                        if(originalreq == TCGETS2) 
                            memcpy((void*)packet->ioctl.arg,node->ioctls[i].pointer_to_struct,sizeof(termios_t));
                        else if(originalreq == TCGETS)
                            memcpy((void*)packet->ioctl.arg,node->ioctls[i].pointer_to_struct,sizeof(termiosold_t));
                        else 
                            memcpy((void*)packet->ioctl.arg,node->ioctls[i].pointer_to_struct,node->ioctls[i].size);
                    } else
                        memcpy((void*)packet->ioctl.arg,node->ioctls[i].pointer_to_struct,node->ioctls[i].size);
                    return 0;
                } else if(node->ioctls[i].write_req == packet->ioctl.ioctlreq) { /* Write */
                    if(node->is_tty) {
                        if(originalreq == TCSETS) {
                            memcpy(node->ioctls[i].pointer_to_struct,(void*)packet->ioctl.arg,sizeof(termiosold_t));
                        } else 
                            memcpy(node->ioctls[i].pointer_to_struct,(void*)packet->ioctl.arg,node->ioctls[i].size);
                            
                    } else
                        memcpy(node->ioctls[i].pointer_to_struct,(void*)packet->ioctl.arg,node->ioctls[i].size);
                    return 0;
                }
            }
            return 0;
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

        case devfs_packet_get_num:

            if(!node)
                return ENOENT;
            
            packet->value = node->dev_num;
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

# define NLDLY  0000400  /* Select newline delays:  */
# define   NL0  0000000  /* Newline type 0.  */
# define   NL1  0000400  /* Newline type 1.  */
# define CRDLY  0003000  /* Select carriage-return delays:  */
# define   CR0  0000000  /* Carriage-return delay type 0.  */
# define   CR1  0001000  /* Carriage-return delay type 1.  */
# define   CR2  0002000  /* Carriage-return delay type 2.  */
# define   CR3  0003000  /* Carriage-return delay type 3.  */
# define TABDLY 0014000  /* Select horizontal-tab delays:  */
# define   TAB0 0000000  /* Horizontal-tab delay type 0.  */
# define   TAB1 0004000  /* Horizontal-tab delay type 1.  */
# define   TAB2 0010000  /* Horizontal-tab delay type 2.  */
# define   TAB3 0014000  /* Expand tabs to spaces.  */
# define BSDLY  0020000  /* Select backspace delays:  */
# define   BS0  0000000  /* Backspace-delay type 0.  */
# define   BS1  0020000  /* Backspace-delay type 1.  */
# define FFDLY  0100000  /* Select form-feed delays:  */
# define   FF0  0000000  /* Form-feed delay type 0.  */
# define   FF1  0100000  /* Form-feed delay type 1.  */

#define OPOST   0000001  /* Post-process output.  */
#define OLCUC   0000002  
#define ONLCR   0000004  /* Map NL to CR-NL on output.  */
#define OCRNL   0000010  /* Map CR to NL on output.  */
#define ONOCR   0000020  /* No CR output at column 0.  */
#define ONLRET  0000040  /* NL performs CR function.  */
#define OFILL   0000100  /* Use fill characters for delay.  */

#define IUCLC   0x0200
#define IXON    0x0400
#define IXOFF   0x1000
#define IMAXBEL 0x2000
#define IUTF8   0x4000
#define CREAD           0x00000080
#define PARENB          0x00000100
#define PARODD          0x00000200
#define HUPCL           0x00000400
#define CLOCAL          0x00000800
#define CBAUDEX         0x00001000
#define BOTHER          0x00001000
#define     B57600      0x00001001
#define    B115200      0x00001002
#define    B230400      0x00001003
#define    B460800      0x00001004
#define    B500000      0x00001005
#define    B576000      0x00001006
#define    B921600      0x00001007
#define   B1000000      0x00001008
#define   B1152000      0x00001009
#define   B1500000      0x0000100a
#define   B2000000      0x0000100b
#define   B2500000      0x0000100c
#define   B3000000      0x0000100d

#define ISIG    0x00001
#define XCASE   0x00004
#define ECHOE   0x00010
#define ECHOK   0x00020
#define ECHONL  0x00040
#define NOFLSH  0x00080
#define TOSTOP  0x00100
#define ECHOCTL 0x00200
#define ECHOPRT 0x00400
#define ECHOKE  0x00800
#define FLUSHO  0x01000
#define PENDIN  0x04000
#define IEXTEN  0x08000
#define EXTPROC 0x10000

int is_early = 1;

int tty_ptr = 0;
std::int32_t __ptmx_open(userspace_fd_t* fd, char* path) {

    memset(fd->path,0,2048);
    __printfbuf(fd->path,2048,"/dev/tty");
    char ttyname[2048];
    memset(ttyname,0,2048);
    __printfbuf(ttyname,2048,"/dev/pts/");

    vfs::devfs_packet_t packet;
    packet.size = tty_ptr;
    packet.request = DEVFS_PACKET_CREATE_PIPE_DEV;
    packet.value = (std::uint64_t)fd->path + 4;

    vfs::devfs::send_packet(ttyname + 4,&packet);

    memset(fd->path,0,2048);
    __printfbuf(fd->path,2048,"/dev/tty%d",tty_ptr);
    memset(ttyname,0,2048);
    __printfbuf(ttyname,2048,"/dev/pts/%d",tty_ptr);

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

    std::uint64_t wins = (uint64_t)memory::pmm::_virtual::alloc(4096);

    packet.request = DEVFS_PACKET_CREATE_IOCTL;
    packet.create_ioctl.pointer = (uint64_t)wins;
    packet.create_ioctl.size = sizeof(struct winsize);
    packet.create_ioctl.readreg = TIOCGWINSZ;
    packet.create_ioctl.writereg = TIOCSWINSZ;

    if(is_early) {
        winsizez ws = Log::GetDimensions();
        memcpy((void*)wins,&ws,sizeof(winsize));
        is_early = 0;
    }

    vfs::devfs::send_packet(ttyname + 4,&packet);

    fd->other_state = USERSPACE_FD_OTHERSTATE_MASTER;
    __printfbuf(fd->path,2048,"/dev/tty%d",tty_ptr);

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

std::int64_t __random_write(userspace_fd_t* fd, void* buffer, std::uint64_t size) {
    vfs::vfs::unlock();
    return size;
}

std::uint64_t next = 2;

std::uint64_t __rand() {
    uint64_t t = __rdtsc();
    return t * (++next);
}

std::int64_t __random_read(userspace_fd_t* fd, void* buffer, std::uint64_t count) {
    vfs::vfs::unlock(); // dont waste time on this
    memset(buffer,0,count);
    for(std::uint64_t i = 0; i < count; i++) {
        ((char*)buffer)[i] = __rand() & 0xFF;
    }
    return count;
}

#include <etc/bootloaderinfo.hpp>

#define FBIOGET_VSCREENINFO	0x4600
#define FBIOPUT_VSCREENINFO	0x4601
#define FBIOGET_FSCREENINFO	0x4602
#define FB_VISUAL_TRUECOLOR		2	/* True color	*/
#define FB_TYPE_PACKED_PIXELS		0	/* Packed Pixels	*/

vfs::devfs_node_t* ktty_node = 0;

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

    const char* name001 = "/urandom";

    packet.size = 0;
    packet.request = DEVFS_PACKET_CREATE_DEV;
    packet.value = (std::uint64_t)name001;

    send_packet((char*)name001,&packet);
    
    __devfs_head->write = __random_write;
    __devfs_head->read = __random_read;

    // liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_DEV << 32) | 0, "/ps2keyboard", "/masterps2keyboard");
    // liborange_setup_ring_bytelen("/ps2keyboard0",1);

    // liborange_create_dev(((uint64_t)DEVFS_PACKET_CREATE_DEV << 32) | 0, "/mouse", "/mastermouse");
    // liborange_setup_ring_bytelen("/mouse0",4);

    packet.size = 0;
    packet.request = DEVFS_PACKET_CREATE_DEV;
    packet.value = (std::uint64_t)"/masterps2keyboard";

    send_packet((char*)"/ps2keyboard",&packet);

    __devfs_head->readring->setup_bytelen(1);
    __devfs_head->writering->setup_bytelen(1);

    packet.size = 0;
    packet.request = DEVFS_PACKET_CREATE_DEV;
    packet.value = (std::uint64_t)"/mastermouse";

    send_packet("/mouse",&packet);

    __devfs_head->readring->setup_bytelen(4);
    __devfs_head->writering->setup_bytelen(4);

    struct limine_framebuffer fb = *BootloaderInfo::AccessFramebuffer();
    
    packet.size = 0;
    packet.request = DEVFS_PACKET_CREATE_DEV;
    packet.value = (std::uint64_t)"/zfb";

    send_packet("/fb",&packet);

    __devfs_head->mmap_base = (std::uint64_t)((std::uint64_t)fb.address - BootloaderInfo::AccessHHDM());
    __devfs_head->mmap_size = fb.pitch * fb.height;
    __devfs_head->mmap_flags = PTE_WC;

    struct fb_var_screeninfo* vinfo = (struct fb_var_screeninfo*)memory::pmm::_virtual::alloc(sizeof(struct fb_var_screeninfo));;
    struct fb_fix_screeninfo* finfo = (struct fb_fix_screeninfo*)memory::pmm::_virtual::alloc(sizeof(struct fb_fix_screeninfo));

    devfs_packet_t packeta;
    packeta.create_ioctl.readreg = FBIOGET_VSCREENINFO;
    packeta.create_ioctl.writereg = 0;
    packeta.create_ioctl.request = DEVFS_PACKET_CREATE_IOCTL;
    packeta.create_ioctl.size = sizeof(struct fb_var_screeninfo);
    packeta.create_ioctl.pointer = (std::uint64_t)vinfo;
    send_packet("/fb0",&packeta);

    packeta.create_ioctl.readreg = FBIOGET_FSCREENINFO;
    packeta.create_ioctl.writereg = 0;
    packeta.create_ioctl.request = DEVFS_PACKET_CREATE_IOCTL;
    packeta.create_ioctl.size = sizeof(struct fb_fix_screeninfo);
    packeta.create_ioctl.pointer = (std::uint64_t)finfo;
    send_packet("/fb0",&packeta);

    vinfo->red.length = fb.red_mask_size;
    vinfo->red.offset = fb.red_mask_shift;
    vinfo->blue.offset = fb.blue_mask_shift;
    vinfo->blue.length = fb.blue_mask_size;
    vinfo->green.length = fb.green_mask_size;
    vinfo->green.offset = fb.green_mask_shift;
    vinfo->xres = fb.width;
    vinfo->yres = fb.height;
    vinfo->bits_per_pixel = fb.bpp < 5 ? (fb.bpp * 8) : fb.bpp;
    vinfo->xres_virtual = fb.width;
    vinfo->yres_virtual = fb.height;
    vinfo->red.msb_right = 1;
    vinfo->green.msb_right = 1;
    vinfo->blue.msb_right = 1;
    vinfo->transp.msb_right = 1;
    vinfo->height = -1;
    vinfo->width = -1;
    finfo->line_length = fb.pitch;
    finfo->smem_len = fb.pitch * fb.height;
    finfo->visual = FB_VISUAL_TRUECOLOR;
    finfo->type = FB_TYPE_PACKED_PIXELS;
    finfo->mmio_len = fb.pitch * fb.height;

    // now init kernel tty
    userspace_fd_t hi;
    memset(&hi,0,sizeof(userspace_fd_t));
    memcpy(hi.path,"/dev/ptmx",sizeof("/dev/ptmx"));
    __ptmx_open(&hi,"/ptmx");

}