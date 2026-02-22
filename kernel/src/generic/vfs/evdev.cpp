
#include <generic/vfs/vfs.hpp>
#include <generic/vfs/devfs.hpp>
#include <generic/vfs/evdev.hpp>
#include <etc/logging.hpp>

#include <cstdint>

vfs::evdev_node* evdev_head_node = 0;

int evdev_num = 0;

inline bool isdigit(char ch) {
    return ch >= '0' && ch <= '9';
}

inline long reverse_number(long num) {
    long reversed_num = 0;
    
    while(num > 0) {
        int digit = num % 10;     
        reversed_num = reversed_num * 10 + digit; 
        num /= 10;                
    }
    
    return reversed_num;
}

vfs::evdev_node* evdev_find_by_num(int num) {
    vfs::evdev_node* dev = evdev_head_node;
    while (dev) {
        if (num == dev->num) {
            return dev;
        } 
        dev = dev->next;
    }

    return 0;
}

vfs::evdev_node* evdev_find_dev(const char* loc) {
    vfs::evdev_node* dev = evdev_head_node;
    char temp[256]; 
    int len = strlen((char*)loc);
    int i = len - 1;

    evdev_num = 0;

    while (i >= 0 && isdigit(loc[i])) {
        evdev_num = evdev_num * 10 + (loc[i] - '0'); 
        i--;
    }

    evdev_num = reverse_number(evdev_num);

    memcpy(temp, loc, i + 1);
    temp[i + 1] = '\0'; 

    while (dev) {
        if (!strcmp(temp,"/event") && evdev_num == dev->num) {
            return dev;
        } 
        dev = dev->next;
    }

    return 0;
}

std::int32_t __evdev__open(userspace_fd_t* fd, char* path) {

    if(path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
        return 0;

    vfs::evdev_node* node = evdev_find_dev(path);
    if(!node)
        return ENOENT;

    if(node->main_ring) {
        fd->queue = node->main_ring->ring.tail;
        fd->cycle = node->main_ring->ring.cycle;
    }

    return 0;
}

std::int32_t __evdev__stat(userspace_fd_t* fd, char* path, vfs::stat_t* out) {
    memset(out,0,sizeof(vfs::stat_t));
    if(path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        out->st_ino = 0;
        out->st_mode |= S_IFDIR;
        return 0;
    }

    vfs::evdev_node* node = evdev_find_dev(path);
    if(!node)
        return -EINVAL;

    out->st_ino = node->num + 1;
    out->st_mode = S_IFCHR;
    return 0;
}

std::int32_t __evdev__statx(userspace_fd_t* fd, char* path, int flags, int mask, statx_t* out) {
    memset(out,0,sizeof(statx_t));

    if(path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        out->stx_mask = STATX_BASIC_STATS;
        out->stx_blksize = 4096;
        out->stx_nlink = 0;
        out->stx_uid = 0;
        out->stx_gid =  0;
        out->stx_mode = S_IFDIR;
        out->stx_ino = 0;
        out->stx_size = 0;
        out->stx_blocks = 0;
        out->stx_atime.tv_sec = 0;
        out->stx_atime.tv_nsec = 0;
        out->stx_btime.tv_sec = 0;
        out->stx_btime.tv_nsec = 0;
        out->stx_ctime.tv_sec = 0;
        out->stx_ctime.tv_nsec = 0;
        out->stx_mtime.tv_sec = 0;
        out->stx_mtime.tv_nsec = 0;
        out->stx_rdev_major = 0;
        out->stx_rdev_minor = 0;
        out->stx_dev_major = 0;
        out->stx_dev_minor = 0;
        return 0;
    }

    vfs::evdev_node* node = evdev_find_dev(path);
    if(!node)
        return -EINVAL;

    out->stx_mask = STATX_BASIC_STATS;
    out->stx_blksize = 4096;
    out->stx_nlink = 0;
    out->stx_uid = 0;
    out->stx_gid =  0;
    out->stx_mode = S_IFCHR;
    out->stx_ino = node->num + 1;
    out->stx_size = 0;
    out->stx_blocks = 0;
    out->stx_atime.tv_sec = 0;
    out->stx_atime.tv_nsec = 0;
    out->stx_btime.tv_sec = 0;
    out->stx_btime.tv_nsec = 0;
    out->stx_ctime.tv_sec = 0;
    out->stx_ctime.tv_nsec = 0;
    out->stx_mtime.tv_sec = 0;
    out->stx_mtime.tv_nsec = 0;
    out->stx_rdev_major = 0;
    out->stx_rdev_minor = 0;
    out->stx_dev_major = 0;
    out->stx_dev_minor = 0;
    return 0;
}

std::int32_t __evdev__ls(userspace_fd_t* fd, char* path, vfs::dirent_t* out) {
againdev:
    vfs::evdev_node* nod = 0;

    if(fd->rv0 == 0 & fd->rv1 == 0) {
        fd->rv0 = (std::uint64_t)evdev_head_node->next;
        nod = evdev_head_node;
    } else if(fd->rv0 != 0) {
        nod = (vfs::evdev_node*)fd->rv0;
        fd->rv0 = (std::uint64_t)nod->next;
    }

    if(!nod) { 
        out->d_reclen = 0;
        memset(out->d_name,0,sizeof(out->d_name));
        return 0; 
    }

    memset(out->d_name,0,sizeof(out->d_name));
    __printfbuf(out->d_name,1024,"event%d",nod->num);

    out->d_reclen = sizeof(vfs::dirent_t);
    out->d_ino = fd->rv1++;
    out->d_off = 0;
    out->d_type = S_IFCHR;
    fd->offset++;
    return 0;
}

std::int64_t __evdev__read(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count) {
    vfs::evdev_node* node = evdev_find_dev(path);
    if(!node) {
        vfs::vfs::unlock();
        return -ENOENT;
    }

    if(node->main_ring) {
        std::uint64_t c = node->main_ring->receivevals(buffer, count, &fd->cycle, &fd->queue);
        vfs::vfs::unlock();
        return c;
    }
    assert(0,"BROOOOOO");
    return -EFAULT;
}

std::int64_t __evdev__write(userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size) {
    vfs::vfs::unlock();
    return -ENOTSUP;
}

int __bitmap_size(int c) {
    return ALIGNUP(c,8) / 8;
}

std::int32_t __evdev__ioctl(userspace_fd_t* fd, char* path, unsigned long req, void *arg, int *res) {

    vfs::evdev_node* node = evdev_find_dev(path);
    if(!node)
        return -EINVAL;

    std::uint64_t size = _IOC_SIZE(req);

    if (_IOC_DIR(req) == _IOC_READ && (_IOC_NR(req) & ~EV_MAX) == _IOC_NR(EVIOCGBIT(0, 0))) {
        switch(_IOC_NR(req) & EV_MAX) {
        case 0: 
            memcpy(arg,node->ev_bitmap->pool,__bitmap_size(EV_MAX));
            return 0;
        }
        assert(0,"doing req %d",_IOC_NR(req) & EV_MAX);
    } else {
        switch(EVIOC_MASK_SIZE(req)) {
        case EVIOCGNAME(0): 
            memcpy(arg, node->name, size > strlen(node->name) ? size : strlen(node->name));
            return 0;
        };
    }

    assert(0, "ioctl req 0x%p arg 0x%p path %s rev 0x%p",EVIOC_MASK_SIZE(req),arg,path,_IOC_NR(req) & EV_MAX);
}

std::int64_t __evdev__poll(userspace_fd_t* fd, char* path, int operation_type) {

    vfs::evdev_node* node = evdev_find_dev(path);
    if(!node)
        return -EINVAL;

    if(!node->main_ring)
        return 0; // always return will block

    int ret;
    if(operation_type == POLLIN) {
        ret = 0;
        if(node->main_ring->isnotempty((int*)&fd->queue,(char*)&fd->cycle)) {
            ret = 1;
        } 

    } else if(operation_type == POLLOUT) {
        ret = 1;
    } 
    return ret;
}

#define BITS_PER_LONG (8 * sizeof(long))
#define IS_SET(bit, array) ((array[(bit) / BITS_PER_LONG] >> ((bit) % BITS_PER_LONG)) & 1)

int vfs::evdev::create(char* name, int type) {
    evdev_node* new_node = (evdev_node*)memory::pmm::_virtual::alloc(4096);
    new_node->num = evdev_num++;
    memcpy(new_node->path,"/event\0",7);
    new_node->main_ring = new evdev_modif_ring(256);
    memcpy(new_node->name,name,strlen(name) + 1);

    if(type == EVDEV_TYPE_KEYBOARD) {
        new_node->ev_bitmap = new Lists::Bitmap(EV_MAX + 1);
        new_node->ev_bitmap->set(EV_KEY);
        new_node->ev_bitmap->set(EV_REP);
    } else if(type == EVDEV_TYPE_MOUSE) {
        new_node->ev_bitmap = new Lists::Bitmap(EV_MAX + 1);
        new_node->ev_bitmap->set(EV_REL);
        new_node->ev_bitmap->set(EV_SYN);
    }

    new_node->next = evdev_head_node;
    new_node->type = type;
    evdev_head_node = new_node;
    Log::Display(LEVEL_MESSAGE_INFO,"evdev: new device /dev/input/event%d with name \"%s\" and type %d\n",new_node->num,name,type);
    return new_node->num;
}

void vfs::evdev::submit(int num, input_event event) {
    evdev_node* node = evdev_find_by_num(num);
    assert(node, "evdev node with num %d not found",num);
    assert(node->main_ring, "wtf !7&77177171???!?!!?771711771 %d %s 0x%p",num,node->name,node->main_ring);
    node->main_ring->send(event);
}

void vfs::evdev::mount(vfs_node_t* node) {
    node->open = __evdev__open;
    node->stat = __evdev__stat;
    node->statx = __evdev__statx;
    node->ls = __evdev__ls;
    node->read = __evdev__read;
    node->write = __evdev__write;
    node->poll = __evdev__poll;
    node->ioctl = __evdev__ioctl;
}