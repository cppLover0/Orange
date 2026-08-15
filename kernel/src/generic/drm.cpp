#include <generic/vfs.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/drm.hpp>
#include <generic/lock/spinlock.hpp>
#include <utils/errno.hpp>
#include <utils/foreach.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>
#include <drivers/edid.hpp>

locks::spinlock drm_lock;
drm::drm_device* head_drm_device = nullptr;
drm::drm_device root_device = {};
std::atomic<int> drm_device_id = 0;

drm::drm_device* drm_lookup(const char* path) {
    foreach(head_drm_device) {
        if(klibc::strcmp(item->path, path) == 0) 
            return item;
    }
    return 0;
}

void drm::create(drm::drm_device device) {
    drm_lock.lock();
    drm::drm_device* new_device = new drm::drm_device;
    *new_device = device;

    new_device->is_root = false;
    new_device->inode = drm_device_id;

    klibc::__printfbuf(new_device->path, 255, "/card%d", drm_device_id++);
    new_device->next = head_drm_device;
    head_drm_device = new_device;

    if(new_device->type == drm::drm_type::framebuffer) {
        
        new_device->connector_ptr = new std::uint32_t[1];
        new_device->encoder_ptr = new std::uint32_t[1];
        new_device->crtc_ptr = new std::uint32_t[1];

        new_device->connector_count = 1;
        new_device->encoder_count = 1;
        new_device->crtc_count = 1;

        new_device->connector_ptr[0] = drm::allocate();
        new_device->encoder_ptr[0] = drm::allocate();
        new_device->crtc_ptr[0] = drm::allocate();

        framebuffer fb = new_device->fb.access_fb(new_device->ctx);

        auto fb_mode = new drm::drm_structs::drm_mode_modeinfo;
        fb_mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_USERDEF;
        fb_mode->hdisplay = fb.width;
        fb_mode->vdisplay = fb.height;
        fb_mode->vrefresh = 60; // default one

        new_device->mode_ptr = new std::uint32_t[1];
        new_device->mode_count = 1;

        new_device->mode_ptr[0] = drm::allocate();
        drm::_kms.create(new_device->mode_ptr[0], fb_mode);
        
        auto connector = new drm::drm_structs::drm_mode_get_connector;
        connector->connector_type = 0; 
        connector->connector_type_id = drm::unknown_connector_type_allocator.allocate();
        connector->connection = 1;

        std::uint8_t w,h = 0;
        if(edid::get_monitor_size(fb.edid, &w, &h) == true) {
            connector->mm_width = w * 10;
            connector->mm_height = h * 10;
        }

        connector->subpixel = 0;
        

    }

    new_device->magic = 0x1122334455667788;

    log("drm", "new drm device %s %s", drm_type_to_str(new_device->type), new_device->path);

    drm_lock.unlock();
}

std::int32_t drm_readlink(filesystem* fs, char* path, char* buffer) {
    (void)path;
    (void)fs;
    (void)buffer;
    return -EINVAL;
}

signed long drm_write(file_descriptor* file, void* buffer, std::size_t count) {
    (void)file;
    (void)buffer;
    (void)count;
    return -EINVAL;
}

std::int32_t drm_stat(file_descriptor* file, stat* out) {
    auto device = (drm::drm_device*)file->fs_specific.drm_dev;

    if(device->is_root == true) {
        out->st_mode = S_IFDIR | 0666;
        out->st_size = 4096;
        return 0;
    } 

    out->st_mode = S_IFCHR | 0666;
    out->st_ino = device->inode + 2; 

    return 0;
}

signed long drm_read(file_descriptor* file, void* buffer, std::size_t count) {
    (void)file;
    (void)buffer;
    (void)count;

    assert(0, "drm read %s count %lli", file->path, count);
    return 0;
}

std::int32_t drm_mmap(file_descriptor* file, std::int64_t offset, std::uint64_t* out_phys, std::size_t* out_size, std::uint64_t* flags) {
    auto device = (drm::drm_device*)file->fs_specific.drm_dev;

    if(device->is_root == true)
        return -EINVAL;

    if(device->type == drm::drm_type::framebuffer) {
        if(offset == (std::int64_t)drm::drm_structs::drm_other::DRM_CONST_FRAMEBUFFER_OFFSET) {
            drm::framebuffer fb = device->fb.access_fb(device->ctx);
            *out_phys = fb.phys;
            *out_size = ALIGNPAGEUP(fb.height * fb.pitch);
            *flags = PAGING_WC;
        } else {
            log("drm", "warning: wrong offset");
            return -EINVAL;
        }
    }

    assert(0, "vvvbbbbnn51");
    return -EFAULT;
}

void drm_close(file_descriptor* file) {
    (void)file;
    return;
}

void drm_ondup(file_descriptor* file) {
    (void)file;
    return;
}

signed long drm_ls(file_descriptor* file, char* out, std::size_t count) {
    auto device = (drm::drm_device*)file->fs_specific.drm_dev;

    if(device->is_root == false)
        return -ENOTDIR;

    dirent* dir = (dirent*)out;
    if(file->other.ls_pointer == nullptr) {
        return 0;
    } else if(file->other.ls_pointer == (void*)1) {
        file->other.ls_pointer = (void*)head_drm_device;
    }

    drm::drm_device* node = (drm::drm_device*)file->other.ls_pointer;

    if(count < sizeof(dirent) + 1 + klibc::strlen(node->path)) {
        return 0;
    }

    dir->d_ino = node->inode + 2;
    dir->d_reclen = sizeof(dirent) + 1 + klibc::strlen(node->path + 1);
    dir->d_type = DT_CHR;
    dir->d_off = 0;
    klibc::memcpy(dir->d_name, node->path + 1, klibc::strlen(node->path + 1) + 1);

    file->other.ls_pointer = (void*)(node->next);

    return dir->d_reclen;
}

std::int32_t drm_ioctl(file_descriptor* file, std::uint64_t req, void* arg) {
    auto device = (drm::drm_device*)file->fs_specific.drm_dev;

    if(device->is_root == true)
        return -EINVAL;

    log("drm", "ioctl req %lli (0x%lx) arg 0x%p", req, req & 0xff, arg);

    switch((drm::drm_structs::drm_requests)(req & 0xFF)) {

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_GETRESOURCES: {
        auto out = (drm::drm_structs::drm_mode_card_res*)arg;

        if(device->type == drm::drm_type::framebuffer) {
            out->count_fbs = 0; // todo: double buffering for having a lot of fbs
            drm::framebuffer fb = device->fb.access_fb(device->ctx);
            out->min_height = fb.height;
            out->min_width = fb.width;
            out->max_height = fb.height;
            out->max_width = fb.width;
        } else {
            assert(0, "m %s", device->path);
        }

        out->count_crtcs = device->crtc_count;
        out->count_connectors = device->connector_count;
        out->count_encoders = device->encoder_count;

        if((void*)out->crtc_id_ptr != nullptr) 
            klibc::memcpy((void*)out->crtc_id_ptr, device->crtc_ptr, out->count_crtcs * sizeof(std::uint32_t));

        if((void*)out->encoder_id_ptr != nullptr) 
            klibc::memcpy((void*)out->encoder_id_ptr, device->encoder_ptr, out->count_encoders * sizeof(std::uint32_t));

        if((void*)out->connector_id_ptr != nullptr) 
            klibc::memcpy((void*)out->connector_id_ptr, device->connector_ptr, out->count_connectors * sizeof(std::uint32_t));

        return 0;
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_GET_CAP: {
        auto out = (drm::drm_structs::drm_get_cap*)arg;

        if(device->type == drm::drm_type::framebuffer) {
            
            switch((drm::drm_structs::drm_caps)out->capability) {
            case drm::drm_structs::drm_caps::DRM_CAP_DUMB_BUFFER:
                out->value = 1;
                break;
            
            case drm::drm_structs::drm_caps::DRM_CAP_DUMB_PREFERRED_DEPTH:
                out->value = device->fb.access_fb(device->ctx).bpp;
                break;

            case drm::drm_structs::drm_caps::DRM_CAP_ADDFB2_MODIFIERS:
            case drm::drm_structs::drm_caps::DRM_CAP_PRIME:
            case drm::drm_structs::drm_caps::DRM_CAP_DUMB_PREFER_SHADOW:
            case drm::drm_structs::drm_caps::DRM_CAP_CURSOR_HEIGHT:
            case drm::drm_structs::drm_caps::DRM_CAP_CURSOR_WIDTH:
                out->value = 0;
                break;

            default:
                log("drm", "unimplemented get cap for fb 0x%lx", out->capability);
                goto unimplemented;
            }

        } else {
            assert(0, "mx %s", device->path);
        }

        return 0;

    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_CREATE_DUMB: {
        
        auto out = (drm::drm_structs::drm_mode_create_dumb*)arg;
        if(device->type == drm::drm_type::framebuffer) {
            
            drm::framebuffer fb = device->fb.access_fb(device->ctx);

            if(out->width != fb.width || out->height != fb.height || out->bpp != fb.bpp) {
                assert(0, "shitt");
            }

            out->size = ALIGNPAGEUP(fb.height * fb.pitch);
            out->handle = drm::allocate();
            drm::_kms.create(out->handle, device);
            out->pitch = fb.pitch;

            return 0;

        } else {
            assert(0, "123");
        }
        
        return 0;
    }
    
    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_ADDFB: {
        auto out = (drm::drm_structs::drm_mode_fb_cmd*)arg;
        if(device->type == drm::drm_type::framebuffer) {
            // just check for errors 

            drm::framebuffer fb = device->fb.access_fb(device->ctx);

            if(fb.width != out->width || fb.height != out->height || fb.pitch != out->pitch || fb.bpp != out->bpp)
                assert(0,"fb fail");

            drm::drm_device* info = (drm::drm_device*)drm::_kms.get(out->handle);

            if(info == nullptr)
                assert(0,"fb fail");

            if(info->magic != 0x1122334455667788)
                assert(0,"fb fail");

            out->fb_id = drm::allocate();

            return 0;

            assert(0,"fb fail");
            return -EINVAL;

        } else {
            assert(0, "./bsdvhnnx");
        }

        return 0;
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_RMFB:
        return 0;

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_DESTROY_DUMB:
        return 0;

    case drm::drm_structs::drm_requests::DRM_IOCTL_VERSION: {
        auto out = (drm::drm_structs::drm_version*)arg;
        if(device->type == drm::drm_type::framebuffer) {
            
            out->version_major = 1;
            out->version_minor = 0;
            out->version_patchlevel = 0;

            const char* name = "simpledrm";
            const char* desc = "meow meow meow";
            const char* date = "20260812";

            if(out->name != nullptr)
                klibc::memcpy(out->name, name, klibc::strlen(name) + 1);
            
            if(out->desc != nullptr)
                klibc::memcpy(out->desc, desc, klibc::strlen(desc) + 1);
            
            if(out->date != nullptr)
                klibc::memcpy(out->date, date, klibc::strlen(date) + 1);

            out->name_len = klibc::strlen(name) + 1;
            out->desc_len = klibc::strlen(desc) + 1;
            out->date_len = klibc::strlen(date) + 1;

            return 0;

        } else {
            assert(0, "./bsdvhnbnnnnx");
        }
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_SET_CLIENT_CAP: {
        if(device->type == drm::drm_type::framebuffer) {
            return -EINVAL;
        } else {
            assert(0, "sigma trollface gigachad edit");
        }
    }

    default:
        goto unimplemented;
    }

unimplemented:
    assert(0, "unimplemented drm ioctl for card %s, req %lli (0x%lx) (0x%lx), arg 0x%p", device->path, req, req, req & 0xFF, arg);
    return -EFAULT;
}

std::int32_t drm_open(filesystem* fs, void* file_desc, char* path, bool is_directory) {
    (void)fs;
    if(klibc::strcmp(path, "/\0") == 0) {
        auto file = (file_descriptor*)file_desc;
        file->fs_specific.drm_dev = &root_device;
        file->other.ls_pointer = (void*)1;
        file->vnode.ls = drm_ls;
        file->vnode.stat = drm_stat;
        return 0;
    }
    
    if(is_directory == true)
        return -ENOTDIR;

    drm::drm_device* device = drm_lookup(path);
    if(device == nullptr)
        return -ENOENT;

    auto file = (file_descriptor*)file_desc;
    file->fs_specific.drm_dev = device;

    file->vnode.ls = drm_ls;
    file->vnode.stat = drm_stat;
    file->vnode.write = drm_write;
    file->vnode.read = drm_read;
    file->vnode.advanced_mmap = drm_mmap;
    file->vnode.ondup = drm_ondup;
    file->vnode.close = drm_close;
    file->vnode.ioctl = drm_ioctl;

    return 0;
}

void drm::init(vfs::node* node) {

    drm::unknown_connector_type_allocator.ptr = 1;

    filesystem* new_fs = new filesystem;
    node->fs = new_fs;

    klibc::memcpy(node->path, "/dev/dri/\0\0", sizeof("/dev/dri/\0\0") + 1);
    klibc::memcpy(node->internal_path, "/dev/dri", sizeof("/dev/dri\0") + 1);
    node->fs->readlink = drm_readlink;
    node->fs->open = drm_open;

    root_device.is_root = true;

    log("drm", "drm path %s", node->path);

}