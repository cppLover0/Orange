#include <cstdint>
#include <generic/evdev.hpp>
#include <generic/vfs.hpp>
#include <klibc/string.hpp>
#include <generic/lock/mutex.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <utils/align.hpp>
#include <utils/errno.hpp>
#include <utils/assert.hpp>

locks::mutex evdev_lock;
evdev::evdev_node evroot_node = {};
evdev::evdev_node* evhead_node = nullptr;
std::uint32_t evdev_ptr = 0;

evdev::evdev_node* evdev_lookup(char* path) {
    evdev::evdev_node* current = evhead_node;
    while(current) {
        if(klibc::strcmp(current->name, path) == 0) {
            return current;
        }
    }
    return nullptr;
}

int __bitmap_size(int c) {
    return ALIGNUP(c,8) / 8;
}

bool evdev_poll(file_descriptor* file, vfs_poll_type type) {
    evdev::evdev_node* node = (evdev::evdev_node*)(file->fs_specific.tmpfs_pointer);
    assert(node->main_ring, "say gex !!!11!11151");
    if(type == vfs_poll_type::pollin) {
        return node->main_ring->is_not_empty(file->other.queue, file->other.cycle);
    } else if(type == vfs_poll_type::pollout) {
        return true;
    }
    return false;
}

std::int32_t evdev_ioctl(file_descriptor* file, std::uint64_t req, void* arg) {

    evdev::evdev_node* node = (evdev::evdev_node*)(file->fs_specific.tmpfs_pointer);

    std::uint64_t size = _IOC_SIZE(req);

    if (_IOC_DIR(req) == _IOC_READ && (_IOC_NR(req) & ~EV_MAX) == _IOC_NR(EVIOCGBIT(0, 0))) {
        switch(_IOC_NR(req) & EV_MAX) {
        case 0: 
            klibc::memcpy(arg,node->ev_bitmap->pool,__bitmap_size(EV_MAX));
            return 0;
        }
        assert(0,"doing req %d",_IOC_NR(req) & EV_MAX);
    } else {
        switch(EVIOC_MASK_SIZE(req)) {
        case EVIOCGNAME(0): 
            klibc::memcpy(arg, node->name, size > (std::uint64_t)klibc::strlen(node->name) ? size : (std::uint64_t)klibc::strlen(node->name));
            return 0;
        };
    }

    assert(0, "ioctl req 0x%p arg 0x%p path %s rev 0x%p",EVIOC_MASK_SIZE(req),arg,node->path,_IOC_NR(req) & EV_MAX);
    return -EFAULT;
}

signed long evdev_read(file_descriptor* file, void* buffer, std::size_t count) {
    evdev::evdev_node* node = (evdev::evdev_node*)(file->fs_specific.tmpfs_pointer);
    assert(node->main_ring,"FAFASF");

    return node->main_ring->receive((input_event*)buffer, count, &file->other.cycle, &file->other.queue);
}

void evdev::submit(int num, input_event event) {
    evdev_lock.lock();
    evdev::evdev_node* node = evhead_node;
    while(node) {
        if(node->num == num)
            break;
    }
    assert(node, "hlehlpehspgfdkjfs");
    assert(node->main_ring,"ffff");

    node->main_ring->send(event);
}

int evdev::create(char* name, int type) {

    evdev_lock.lock();
    evdev::evdev_node* new_node = (evdev::evdev_node*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_node->main_ring = new utils::ring_buffer<input_event>(256);

    if(type == EVDEV_TYPE_KEYBOARD) {
        new_node->ev_bitmap = new utils::bitmap(EV_MAX + 1);
        new_node->ev_bitmap->set(EV_KEY);
        new_node->ev_bitmap->set(EV_REP);
    } else if(type == EVDEV_TYPE_MOUSE) {
        new_node->ev_bitmap = new utils::bitmap(EV_MAX + 1);
        new_node->ev_bitmap->set(EV_REL);
        new_node->ev_bitmap->set(EV_SYN);
    }

    new_node->num = ++evdev_ptr;
    klibc::__printfbuf(new_node->path, 256, "/event%d\r\n",new_node->num);
    klibc::memcpy(new_node->name, name, klibc::strlen(name));

    new_node->type = type;
    new_node->next = evhead_node;
    evhead_node = new_node;
    return new_node->num;
}

std::int32_t evdev_readlink(filesystem* fs, char* path, char* buffer) {
    (void)fs;
    (void)path;
    (void)buffer;
    return -EINVAL;
}

std::int32_t evdev_create(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode) {
    (void)fs;
    (void)path;
    (void)type;
    (void)mode;
    return -ENOTSUP;
}

signed long evdev_write(file_descriptor* file, void* buffer, std::size_t count) {
    (void)file;
    (void)buffer;
    (void)count;
    return -EACCES;
}

std::int32_t evdev_stat(file_descriptor* file, stat* out) {
    evdev::evdev_node* node = (evdev::evdev_node*)(file->fs_specific.tmpfs_pointer);

    klibc::memset(out, 0, sizeof(stat));

    if(node->is_root) {
        out->st_mode = S_IFDIR | 0666;
        return 0;
    }

    out->st_size = 0;
    out->st_blksize = 0;
    out->st_blocks = 0;
    out->st_mode = S_IFCHR | 0666;
    out->st_ino = node->num;
    return 0;
}

std::int32_t evdev_open(filesystem* fs, void* file_desc, char* path, bool is_directory) {

    file_descriptor* fd = (file_descriptor*)file_desc;
    (void)fs;

    if(klibc::strcmp(path, "/") == 0) {
        fd->fs_specific.tmpfs_pointer = (std::uint64_t)&evroot_node;
        return 0;
    }

    evdev_lock.lock();

    if(is_directory)
        return -EISDIR;

    evdev::evdev_node* node = evdev_lookup(path);
    fd->fs_specific.tmpfs_pointer = (std::uint64_t)node;
    fd->vnode.read = evdev_read;
    fd->vnode.ioctl = evdev_ioctl;
    fd->vnode.write = evdev_write;
    fd->vnode.stat = evdev_stat;

    evdev_lock.unlock();
    return 0;
}

void evdev::init_default(vfs::node* node) {
    evroot_node.is_root = true;
    klibc::memcpy(node->path, "/dev/input/\0\0", sizeof("/dev/input/\0\0") + 1);
    node->fs->readlink = evdev_readlink;
    node->fs->open = evdev_open;
    node->fs->create = evdev_create;
    
}