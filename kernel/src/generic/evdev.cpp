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

locks::spinlock evdev_lock;
evdev::evdev_node evroot_node = {};
evdev::evdev_node* evhead_node = nullptr;
std::uint32_t evdev_ptr = 0;

evdev::evdev_node* evdev_lookup(char* path) {
    evdev::evdev_node* current = evhead_node;
    while(current) {
        if(klibc::strcmp(current->path, path) == 0) {
            return current;
        } 
        current = current->next;
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

    switch (req) {
    case EVIOCGVERSION:
        *(int*)arg = 0x010001;
        return 0;
    case EVIOCGID:
        klibc::memset(arg, 0, sizeof(struct input_id));
        return 0;
    case EVIOCGRAB:
        return 0;
    }

    std::uint64_t size = _IOC_SIZE(req);

    if (_IOC_DIR(req) == _IOC_READ && (_IOC_NR(req) & ~EV_MAX) == _IOC_NR(EVIOCGBIT(0, 0))) {
        switch(_IOC_NR(req) & EV_MAX) {
        case 0: 
            klibc::memcpy(arg,node->ev_bitmap->pool,__bitmap_size(EV_MAX));
            return 0;
        case 1: 
            klibc::memcpy(arg,node->key_bitmap->pool,__bitmap_size(KEY_MAX));
            return 0;
        case 2: 
            klibc::memcpy(arg,node->rel_bitmap->pool,__bitmap_size(REL_MAX));
            return 0;
        case 3:
            klibc::memset(arg, 0 ,__bitmap_size(ABS_MAX));
            return 0;
        case 17:
            klibc::memset(arg, 0 ,__bitmap_size(LED_MAX));
            return 0;
        case 5:
            klibc::memset(arg, 0 ,__bitmap_size(SW_MAX));
            return 0;
        case 4:
            klibc::memset(arg, 0 ,__bitmap_size(MSC_MAX));
            return 0;
        case 21:
            klibc::memset(arg, 0 ,__bitmap_size(FF_MAX));
            return 0;
        case 18:
            klibc::memset(arg, 0 ,__bitmap_size(SND_MAX));
            return 0;
        }
        assert(0,"doing req %d %d",_IOC_NR(req) & EV_MAX, req);
    } else {
        switch(EVIOC_MASK_SIZE(req)) {
        case EVIOCGNAME(0): 
            klibc::memcpy(arg, node->name, size > (std::uint64_t)klibc::strlen(node->name) ? size : (std::uint64_t)klibc::strlen(node->name));
            return 0;
        case EVIOCGPROP(0):
            klibc::memset(arg, 0, __bitmap_size(0x20));
            return 0;
        case EVIOCGPHYS(0):
            klibc::memset(arg, 0, size);
            return 0;
        case EVIOCGUNIQ(0):
            klibc::memset(arg, 0, size);
            return 0;
        default:
            klibc::memset(arg, 0, size);
            return 0;
        };
        assert(0, "ioctl no req 0x%p 0x%p 0x%p gid 0x%p 0x%p 0x%p", EVIOC_MASK_SIZE(req), EVIOCGNAME(0), EVIOCGPHYS(0), EVIOCGID, EVIOCGREP, EVIOCSREP);
    }

    assert(0, "ioctl req 0x%p arg 0x%p path %s rev 0x%p",EVIOC_MASK_SIZE(req),arg,node->path,_IOC_NR(req) & EV_MAX);
    return -EFAULT;
}

signed long evdev_read(file_descriptor* file, void* buffer, std::size_t count) {
    evdev::evdev_node* node = (evdev::evdev_node*)(file->fs_specific.tmpfs_pointer);
    assert(node->main_ring,"FAFASF");

    return node->main_ring->receive((input_event*)buffer, count, &file->other.cycle, &file->other.queue);
}

signed long evdev_ls(file_descriptor* file, char* out, std::size_t count) {
    return 0;
    evdev_lock.lock();
    dirent* dir = (dirent*)out;
    if(file->other.ls_pointer == nullptr) {
        evdev_lock.unlock();
        return 0;
    } else if(file->other.ls_pointer == (void*)1) {
        file->other.ls_pointer = (void*)evhead_node;
    }

    evdev::evdev_node* node = (evdev::evdev_node*)file->other.ls_pointer;

    if(count < sizeof(dirent) + 1 + klibc::strlen(node->path)) {
        evdev_lock.unlock();
        return 0;
    }

    dir->d_ino = node->num;
    dir->d_reclen = sizeof(dirent) + 1 + klibc::strlen(node->path + 1);
    dir->d_type = DT_CHR;
    dir->d_off = 0;
    klibc::memcpy(dir->d_name, node->path + 1, klibc::strlen(node->path + 1) + 1);

    file->other.ls_pointer = (void*)(node->next);

    evdev_lock.unlock();
    return dir->d_reclen;
}

void evdev::submit(int num, input_event event) {
    evdev_lock.lock();
    evdev::evdev_node* node = evhead_node;
    while(node) {
        if(node->num == num)
            break;
        node = node->next;
    }
    assert(node, "hlehlpehspgfdkjfs");
    assert(node->main_ring,"ffff");

    node->main_ring->send(event);

    evdev_lock.unlock();
}

int evdev::create(char* name, int type) {
    return 0;

    evdev_lock.lock();
    evdev::evdev_node* new_node = (evdev::evdev_node*)(pmm::freelist::alloc_4k() + etc::hhdm());
    new_node->main_ring = new utils::ring_buffer<input_event>(65565);

    if(type == EVDEV_TYPE_KEYBOARD) {
        new_node->ev_bitmap = new utils::bitmap(EV_MAX + 1);
        new_node->ev_bitmap->set(EV_KEY);
        new_node->ev_bitmap->set(EV_REP);
        new_node->key_bitmap = new utils::bitmap(512);
        for(int key = 1;key < 128;key++)
            if(key != 84)
                new_node->key_bitmap->set(key);

        new_node->rel_bitmap = new utils::bitmap(256);

    } else if(type == EVDEV_TYPE_MOUSE) {
        new_node->ev_bitmap = new utils::bitmap(EV_MAX + 1);
        new_node->ev_bitmap->set(EV_REL);
        new_node->ev_bitmap->set(EV_KEY);
        new_node->ev_bitmap->set(EV_SYN);
        new_node->key_bitmap = new utils::bitmap(512);
        new_node->key_bitmap->set(BTN_LEFT);
        new_node->key_bitmap->set(BTN_RIGHT);
        new_node->key_bitmap->set(BTN_MIDDLE);
        new_node->rel_bitmap = new utils::bitmap(256);
        new_node->rel_bitmap->set(0);
        new_node->rel_bitmap->set(1);
    }

    new_node->num = ++evdev_ptr;
    klibc::__printfbuf(new_node->path, 256, "/event%d\0",new_node->num);
    klibc::memcpy(new_node->name, name, klibc::strlen(name));

    new_node->type = type;
    new_node->next = evhead_node;
    evhead_node = new_node;
    evdev_lock.unlock();
    return new_node->num;
}

std::int32_t evdev_readlink(filesystem* fs, char* path, char* buffer) {
    (void)fs;
    (void)path;
    (void)buffer;
    return -EINVAL;
}

std::int32_t evdev_create(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode, int uid, int gid) {
    (void)uid;
    (void)gid;
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
    return count;
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
        fd->vnode.ls = evdev_ls;
        fd->vnode.stat = evdev_stat;
        fd->other.ls_pointer = (void*)1;
        return 0;
    }

    evdev_lock.lock();

    if(is_directory) { evdev_lock.unlock();
        return -EISDIR; 
    }

    evdev::evdev_node* node = evdev_lookup(path);
    if(node == nullptr)
        return -ENOENT;
    
    fd->fs_specific.tmpfs_pointer = (std::uint64_t)node;
    fd->vnode.read = evdev_read;
    fd->vnode.ioctl = evdev_ioctl;
    fd->vnode.write = evdev_write;
    fd->vnode.poll = evdev_poll;
    fd->vnode.stat = evdev_stat;
    fd->vnode.ls = evdev_ls;

    fd->other.cycle = node->main_ring->cycle;
    fd->other.queue = node->main_ring->tail;

    evdev_lock.unlock();
    return 0;
}

void evdev::init_default(vfs::node* node) {
    evroot_node.is_root = true;

    filesystem* new_fs = new filesystem;
    node->fs = new_fs;

    klibc::memcpy(node->path, "/dev/input/\0\0", sizeof("/dev/input/\0\0") + 1);
    klibc::memcpy(node->internal_path, "/dev/input", sizeof("/dev/input\0") + 1);
    node->fs->readlink = evdev_readlink;
    node->fs->open = evdev_open;
    node->fs->create = evdev_create;

    log("evdev", "evdev filesystem is 0x%p",new_fs);
    
}