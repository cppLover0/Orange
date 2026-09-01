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

std::uint32_t drm::fb_to_format(drm::framebuffer* fb) {
    if (fb->bpp == 32) {
        if (fb->red_mask_size == 8 && fb->green_mask_size == 8 && fb->blue_mask_size == 8) {
            
            if (fb->red_mask_shift == 16 && fb->green_mask_shift == 8 && fb->blue_mask_shift == 0) {
                return DRM_FORMAT_XRGB8888;
            }
            
            if (fb->blue_mask_shift == 16 && fb->green_mask_shift == 8 && fb->red_mask_shift == 0) {
                return DRM_FORMAT_XBGR8888;
            }
            
            if (fb->blue_mask_shift == 24 && fb->green_mask_shift == 16 && fb->red_mask_shift == 8) {
                return DRM_FORMAT_BGRX8888;
            }
            
            if (fb->red_mask_shift == 24 && fb->green_mask_shift == 16 && fb->blue_mask_shift == 8) {
                return DRM_FORMAT_RGBX8888;
            }
        }
    } else if (fb->bpp == 16) {
        if (fb->red_mask_size == 5 && fb->green_mask_size == 6 && fb->blue_mask_size == 5) {
            if (fb->red_mask_shift == 11 && fb->green_mask_shift == 5 && fb->blue_mask_shift == 0) {
                return DRM_FORMAT_RGB565;
            }
            if (fb->blue_mask_shift == 11 && fb->green_mask_shift == 5 && fb->red_mask_shift == 0) {
                return DRM_FORMAT_BGR565;
            }
        }
    } else if (fb->bpp == 24) {
        if (fb->red_mask_size == 8 && fb->green_mask_size == 8 && fb->blue_mask_size == 8) {
            if (fb->red_mask_shift == 16 && fb->green_mask_shift == 8 && fb->blue_mask_shift == 0) {
                return DRM_FORMAT_RGB888;
            }
            if (fb->blue_mask_shift == 16 && fb->green_mask_shift == 8 && fb->red_mask_shift == 0) {
                return DRM_FORMAT_BGR888;
            }
        }
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

    // ill put simpledrm here, other drivers will have own files 

    if(new_device->type == drm::drm_type::framebuffer) {
        
        new_device->connector_ptr = new std::uint32_t[1];
        new_device->encoder_ptr = new std::uint32_t[1];
        new_device->crtc_ptr = new std::uint32_t[1];
        new_device->plane_ptr = new std::uint32_t[1];

        new_device->connector_count = 1;
        new_device->encoder_count = 1;
        new_device->crtc_count = 1;
        new_device->plane_count = 1;

        new_device->connector_ptr[0] = drm::allocate();
        new_device->encoder_ptr[0] = drm::allocate();
        new_device->crtc_ptr[0] = drm::allocate();

        framebuffer fb = new_device->fb.access_fb(new_device->ctx);

        auto fb_mode = new drm::drm_structs::drm_mode_modeinfo;

        fb_mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_USERDEF;
        fb_mode->hdisplay = fb.width;
        fb_mode->vdisplay = fb.height;
        fb_mode->vrefresh = 60;
        fb_mode->hsync_start = fb.width + 8;
        fb_mode->hsync_end   = fb.width + 16;
        fb_mode->htotal      = fb.width + 24;
        fb_mode->vsync_start = fb.height + 2;
        fb_mode->vsync_end   = fb.height + 4;
        fb_mode->vtotal      = fb.height + 6;
        fb_mode->clock = (fb_mode->htotal * fb_mode->vtotal * fb_mode->vrefresh) / 1000;
        fb_mode->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;

        klibc::__printfbuf(fb_mode->name, 32, "%dx%d", fb.width, fb.height);

        std::uint32_t* encoder_magic = new std::uint32_t;

        *encoder_magic = 0x0DDEAC0D;

        drm::_kms.create(new_device->encoder_ptr[0], encoder_magic);

        new_device->mode_ptr = new std::uint32_t[1];
        new_device->mode_count = 1;

        new_device->current_mode = *fb_mode;

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
        connector->modes_ptr = (std::uint64_t)fb_mode;
        connector->count_modes = 1;

        auto connector_props = new drm_structs::drm_mode_get_property[4];
        auto connector_props_values = new std::uint64_t[4];

        drm::fill_property("CRTC_ID", DRM_MODE_PROP_OBJECT, &connector_props[0]);
    
        // "link-status": enum {"Good", "Bad"} = Good 
        drm::drm_enum link_status_enum[2] = {
            {0, "Bad"},
            {1, "Good"}
        };

        drm::fill_property("link-status", DRM_MODE_PROP_ENUM, &connector_props[1], link_status_enum, 2);

        // "non-desktop" (immutable): range [0, 1] = 0 
        auto non_desktop_values = new std::uint64_t[2];
        non_desktop_values[0] = 0;
        non_desktop_values[1] = 1;

        drm::fill_property("non-desktop", DRM_MODE_PROP_RANGE, &connector_props[2], non_desktop_values, 2);

        // "DPMS": enum {"On", "Standby", "Suspend", "Off"} = On 
        drm::drm_enum dpms_enum[4] = {
            {0, "Off"},
            {1, "Suspend"},
            {2, "Standby"}, 
            {3, "On"}
        };
 
        drm::fill_property("DPMS", DRM_MODE_PROP_ENUM, &connector_props[3], dpms_enum, 4);

        connector_props_values[0] = new_device->crtc_ptr[0];
        connector_props_values[1] = 1;
        connector_props_values[2] = 0;
        connector_props_values[3] = 3;

        auto connector_props_id = new std::uint32_t[4];

        connector_props_id[0] = drm::allocate();
        connector_props_id[1] = drm::allocate();
        connector_props_id[2] = drm::allocate();
        connector_props_id[3] = drm::allocate();

        drm::_kms.create(connector_props_id[0], &connector_props[0]);
        drm::_kms.create(connector_props_id[1], &connector_props[1]);
        drm::_kms.create(connector_props_id[2], &connector_props[2]);
        drm::_kms.create(connector_props_id[3], &connector_props[3]);

        connector->count_props = 4;
        connector->props_ptr = (std::uint64_t)connector_props_id;
        connector->prop_values_ptr = (std::uint64_t)connector_props_values;

        connector->encoder_id = new_device->encoder_ptr[0];
        connector->count_encoders = 1;
        connector->encoders_ptr = (std::uint64_t)new_device->encoder_ptr;

        drm::_kms.create(new_device->connector_ptr[0], (void*)connector);

        auto crtc = new drm::drm_structs::generic_crtc;

        auto crtc_props = new drm_structs::drm_mode_get_property[4];
        auto crtc_props_values = new std::uint64_t[4];

        // "ACTIVE" (atomic): range [0, 1] = 1 
        auto active_crtc_range = new std::uint64_t[2];
        active_crtc_range[0] = 0;
        active_crtc_range[1] = 1;

        drm::fill_property("ACTIVE", DRM_MODE_PROP_RANGE, &crtc_props[0], active_crtc_range, 2);

        // "MODE_ID" (atomic): blob = fb_id
        drm::fill_property("MODE_ID", DRM_MODE_PROP_BLOB, &crtc_props[1]);

        // "OUT_FENCE_PTR" (atomic): range [0, UINT64_MAX] = 0 
        auto out_fence_ptr_range = new std::uint64_t[2];
        out_fence_ptr_range[0] = 0;
        out_fence_ptr_range[1] = 0xffffffffffffffff;

        drm::fill_property("OUT_FENCE_PTR", DRM_MODE_PROP_RANGE, &crtc_props[2], out_fence_ptr_range, 2);

        // "VRR_ENABLED": range [0, 1] = 0 
        auto vrr_enabled_range = new std::uint64_t[2];
        vrr_enabled_range[0] = 0;
        vrr_enabled_range[1] = 1;

        drm::fill_property("VRR_ENABLED", DRM_MODE_PROP_RANGE, &crtc_props[3], vrr_enabled_range, 2);
        
        auto crtc_props_id = new std::uint32_t[4];

        crtc_props_id[0] = drm::allocate();
        crtc_props_id[1] = drm::allocate();
        crtc_props_id[2] = drm::allocate();
        crtc_props_id[3] = drm::allocate();

        drm::_kms.create(crtc_props_id[0], &crtc_props[0]);
        drm::_kms.create(crtc_props_id[1], &crtc_props[1]);
        drm::_kms.create(crtc_props_id[2], &crtc_props[2]);
        drm::_kms.create(crtc_props_id[3], &crtc_props[3]);

        crtc_props_values[0] = 1;
        crtc_props_values[1] = new_device->mode_ptr[0];
        crtc_props_values[2] = 0;
        crtc_props_values[3] = 0;

        crtc->prop_values_ptr = crtc_props_values;
        crtc->props_ptr = crtc_props_id;
        crtc->props_count = 4;
        crtc->magic = 0xDEAD1234;

        drm::_kms.create(new_device->crtc_ptr[0], crtc);

        auto plane = new drm::drm_structs::drm_mode_get_plane;

        plane->gamma_size = 0;
        plane->crtc_id = new_device->crtc_ptr[0];
        plane->count_format_types = 1;
        plane->possible_crtcs = 1;

        auto format_type_ptr = new std::uint32_t[1];
        format_type_ptr[0] = fb_to_format(&fb);

        log("drm", "framebuffer format 0x%p", format_type_ptr[0]);

        plane->format_type_ptr = (std::uint64_t)format_type_ptr;
        plane->magic = 0x8934AAFFBB91001B;

        new_device->plane_ptr[0] = drm::allocate();
        drm::_kms.create(new_device->plane_ptr[0], plane);

        /*
        
            "CRTC_H" (atomic): range [0, INT32_MAX] = 768
            "CRTC_ID" (atomic): object CRTC = 35
            "CRTC_W" (atomic): range [0, INT32_MAX] = 1024
            "CRTC_X" (atomic): signed range [INT32_MIN, INT32_MAX] = 0
            "CRTC_Y" (atomic): signed range [INT32_MIN, INT32_MAX] = 0
            "FB_ID" (atomic): object FB = 41
            "IN_FENCE_FD" (atomic): signed range [-1, INT32_MAX] = -1
            "SRC_H" (atomic): range [0, UINT32_MAX] = 768
            "SRC_W" (atomic): range [0, UINT32_MAX] = 1024
            "SRC_X" (atomic): range [0, UINT32_MAX] = 0
            "SRC_Y" (atomic): range [0, UINT32_MAX] = 0
            "type" (immutable): enum {"Overlay", "Primary", "Cursor"} = Primary 
        
        */

        auto plane_props = new drm_structs::drm_mode_get_property[12];
        auto plane_props_values = new std::uint64_t[12];

        plane_props_values[0] = 768;
        plane_props_values[1] = new_device->crtc_ptr[0];
        plane_props_values[2] = 1024;
        plane_props_values[3] = 0;
        plane_props_values[4] = 0;
        plane_props_values[5] = 0;
        plane_props_values[6] = -1;
        plane_props_values[7] = 768;
        plane_props_values[8] = 1024;
        plane_props_values[9] = 0;
        plane_props_values[10] = 0;
        plane_props_values[11] = 1;

        auto signed_range = new std::uint64_t[2];
        signed_range[0] = 0;
        signed_range[1] = 2147483647; // int32_max
        
        auto signed_negative_range = new std::uint64_t[2];
        signed_negative_range[0] = -2147483648; // int32_min
        signed_negative_range[1] = 2147483647; // int32_max

        auto unsigned_range = new std::uint64_t[2];
        unsigned_range[0] = 0;
        unsigned_range[1] = 0xffffffff; // uint32_max

        auto negative_one_range = new std::uint64_t[2];
        negative_one_range[0] = -1;
        negative_one_range[1] = 2147483647; // int32_max

        drm::fill_property("CRTC_H", DRM_MODE_PROP_RANGE, &plane_props[0], signed_range, 2);
        drm::fill_property("CRTC_ID", DRM_MODE_PROP_OBJECT, &plane_props[1]);
        drm::fill_property("CRTC_W", DRM_MODE_PROP_RANGE, &plane_props[2], signed_range, 2);
        drm::fill_property("CRTC_X", DRM_MODE_PROP_SIGNED_RANGE, &plane_props[3], signed_negative_range, 2);
        drm::fill_property("CRTC_Y", DRM_MODE_PROP_SIGNED_RANGE, &plane_props[4], signed_negative_range, 2);
        drm::fill_property("FB_ID", DRM_MODE_PROP_OBJECT, &plane_props[5]);
        drm::fill_property("IN_FENCE_FD", DRM_MODE_PROP_SIGNED_RANGE, &plane_props[6], negative_one_range, 2);
        drm::fill_property("SRC_H", DRM_MODE_PROP_RANGE, &plane_props[7], unsigned_range, 2);
        drm::fill_property("SRC_W", DRM_MODE_PROP_RANGE, &plane_props[8], unsigned_range, 2);
        drm::fill_property("SRC_X", DRM_MODE_PROP_RANGE, &plane_props[9], unsigned_range, 2);
        drm::fill_property("SRC_Y", DRM_MODE_PROP_RANGE, &plane_props[10], unsigned_range, 2);

        // "type" (immutable): enum {"Overlay", "Primary", "Cursor"} = Primary 
        drm::drm_enum plane_type_enum[] = {
            {0, "Overlay"},
            {1, "Primary"},
            {2, "Cursor"}
        };
        
        drm::fill_property("type", DRM_MODE_PROP_ENUM, &plane_props[11], plane_type_enum, 3);

        auto plane_props_id = new std::uint32_t[12];

        plane_props_id[0] = drm::allocate();
        plane_props_id[1] = drm::allocate();
        plane_props_id[2] = drm::allocate();
        plane_props_id[3] = drm::allocate();
        plane_props_id[4] = drm::allocate();
        plane_props_id[5] = drm::allocate();
        plane_props_id[6] = drm::allocate();
        plane_props_id[7] = drm::allocate();
        plane_props_id[8] = drm::allocate();
        plane_props_id[9] = drm::allocate();
        plane_props_id[10] = drm::allocate();
        plane_props_id[11] = drm::allocate();

        drm::_kms.create(plane_props_id[0], &plane_props[0]);
        drm::_kms.create(plane_props_id[1], &plane_props[1]);
        drm::_kms.create(plane_props_id[2], &plane_props[2]);
        drm::_kms.create(plane_props_id[3], &plane_props[3]);
        drm::_kms.create(plane_props_id[4], &plane_props[4]);
        drm::_kms.create(plane_props_id[5], &plane_props[5]);
        drm::_kms.create(plane_props_id[6], &plane_props[6]);
        drm::_kms.create(plane_props_id[7], &plane_props[7]);
        drm::_kms.create(plane_props_id[8], &plane_props[8]);
        drm::_kms.create(plane_props_id[9], &plane_props[9]);
        drm::_kms.create(plane_props_id[10], &plane_props[10]);
        drm::_kms.create(plane_props_id[11], &plane_props[11]);

        new_device->plane_prop_values = plane_props_values;
        new_device->plane_props = plane_props_id;
        new_device->plane_prop_count = 12;
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

    (void)file;
    (void)offset;
    (void)out_phys;
    (void)out_size;
    (void)flags;

    auto info = (drm::drm_structs::drm_dumbbuffer*)drm::_kms.get(offset);

    if(info == nullptr)
        assert(0,"fb f7ailcM");

    if(info->magic != 0x78888810ABCD)
        assert(0,"fb fail8vM");

    *out_phys = info->phys;
    *out_size = info->len;
    *flags = PAGING_WC;

    log("drm", "mmaping drm phys 0x%p, handle %d", info->phys, offset);

    return 0;

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

    //log("drm", "ioctl req %lli (0x%lx) arg 0x%p", req, req & 0xff, arg);

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
            case drm::drm_structs::drm_caps::DRM_CAP_ASYNC_PAGE_FLIP:
                out->value = 0;
                break;
            case drm::drm_structs::drm_caps::DRM_CAP_CURSOR_HEIGHT:
            case drm::drm_structs::drm_caps::DRM_CAP_CURSOR_WIDTH:
                out->value = 64;
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
            (void)fb;

            out->pitch = (out->width * out->bpp + 7) / 8;
            out->size = out->pitch * out->height;
            out->size = ALIGNPAGEUP(out->size);

            out->handle = drm::allocate();

            auto dumb = new drm::drm_structs::drm_dumbbuffer;
            dumb->magic = 0x78888810ABCD;
            dumb->phys = pmm::buddy::alloc(out->size).phys;
            dumb->len = out->size;

            drm::_kms.create(out->handle, dumb);

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
                assert(0,"fb fail %d %d %d %d %d %d %d %d", fb.width, out->width, fb.height, out->height, fb.pitch, out->pitch, fb.bpp, out->bpp);

            auto info = (drm::drm_structs::drm_dumbbuffer*)drm::_kms.get(out->handle);

            if(info == nullptr)
                assert(0,"fb failc");

            if(info->magic != 0x78888810ABCD)
                assert(0,"fb failv");

            out->fb_id = drm::allocate();

            drm::_kms.create(out->fb_id, info);

            info->is_fb = true;

            return 0;

        } else {
            assert(0, "./bsdvhnnx");
        }

        return 0;
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_RMFB:
        return 0;

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_MAP_DUMB: {
        auto in = (drm::drm_structs::drm_mode_map_dumb*)arg;
        if(device->type == drm::drm_type::framebuffer) {

            auto info = (drm::drm_structs::drm_dumbbuffer*)drm::_kms.get(in->handle);

            if(info == nullptr)
                assert(0,"fb f7ailc");

            if(info->magic != 0x78888810ABCD)
                assert(0,"fb fail8v");

            in->offset = in->handle;

            return 0;
        }
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_DESTROY_DUMB: {
        auto out = (drm::drm_structs::drm_mode_destroy_dumb*)arg;
        if(device->type == drm::drm_type::framebuffer) {
            // just check for errors 

            auto info = (drm::drm_structs::drm_dumbbuffer*)drm::_kms.get(out->handle);

            if(info == nullptr)
                assert(0,"fb f7ailc");

            if(info->magic != 0x78888810ABCD)
                assert(0,"fb fail8v");

            if(info->phys != 0) {
                pmm::buddy::free(info->phys);
            }

            return 0;

        } else {
            assert(0, "./bsdvhn00nx");
        }
        return 0;
    }

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

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_GETCONNECTOR: {
        
        if(device->type == drm::drm_type::framebuffer) {

            auto source = (drm::drm_structs::drm_mode_get_connector*)arg;
            auto info = (drm::drm_structs::drm_mode_get_connector*)drm::_kms.get(source->connector_id);

            if(info == nullptr)
                assert(0,"conn fail %d", source->connector_id);

            if((void*)source->encoders_ptr != nullptr) {
                klibc::memcpy((void*)source->encoders_ptr, (void*)info->encoders_ptr, source->count_encoders * sizeof(std::uint32_t));
            } else {
                source->count_encoders = info->count_encoders;
            }

            if((void*)source->modes_ptr != nullptr) {
                klibc::memcpy((void*)source->modes_ptr, (void*)info->modes_ptr, source->count_modes * sizeof(drm::drm_structs::drm_mode_modeinfo));
            } else {
                source->count_modes = info->count_modes;
            }

            if((void*)source->prop_values_ptr != nullptr) {
                klibc::memcpy((void*)source->prop_values_ptr, (void*)info->prop_values_ptr, source->count_props * sizeof(std::uint64_t));
            }

            if((void*)source->props_ptr != nullptr) {
                klibc::memcpy((void*)source->props_ptr, (void*)info->props_ptr, source->count_props * sizeof(std::uint32_t));
            } else {
                source->count_props = info->count_props;
            }

            source->encoder_id = info->encoder_id;
            source->connector_type = info->connector_type;
            source->connector_type_id = info->connector_type_id;
            source->connection = info->connection;
            source->mm_width = info->mm_width;
            source->mm_height = info->mm_height;
            source->subpixel = info->subpixel;

            return 0;

        } else {
            assert(0, "sigma trollface gigachad editX");
        }
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_GETPROPERTY: {

        auto prop = (drm::drm_structs::drm_mode_get_property*)arg;
        auto info = (drm::drm_structs::drm_mode_get_property*)drm::_kms.get(prop->prop_id);

        if(info == nullptr)
            assert(0,"gigachad edit prop тгдд");

        if(info->magic != 0x666111)
            assert(0,"gigachad edit prop");

        if((void*)info->values_ptr != nullptr) {
            if((void*)prop->values_ptr != nullptr) {
                klibc::memcpy((void*)prop->values_ptr, (void*)info->values_ptr, prop->count_values * sizeof(std::uint64_t));
            } else {
                prop->count_values = info->count_values;
            }
        } 

        if((void*)info->enum_blob_ptr != nullptr) {
            if((void*)prop->enum_blob_ptr != nullptr) {
                klibc::memcpy((void*)prop->enum_blob_ptr, (void*)info->enum_blob_ptr, prop->count_enum_blobs * sizeof(drm::drm_structs::drm_mode_property_enum));
            } else {
                prop->count_enum_blobs = info->count_enum_blobs;
            }
        }

        prop->flags = info->flags;

        klibc::memcpy(prop->name, info->name, sizeof(info->name));
        return 0;
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_SET_CLIENT_CAP: {
        if(device->type == drm::drm_type::framebuffer) {
            return -EINVAL;
        } else {
            assert(0, "sigma trollface gigachad edit");
        }
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_DIRTYFB: {
        auto in = (drm::drm_structs::drm_mode_fb_dirty_cmd*)arg;
        if(device->type == drm::drm_type::framebuffer) {
            auto info = (drm::drm_structs::drm_dumbbuffer*)drm::_kms.get(in->fb_id);

            if(info == nullptr) {
                log("drm", "failed get fb that used for dirtyfb");
                return 0;
            }

            if(info->magic != 0x78888810ABCD)
                assert(0, "fb failv");

            drm::framebuffer fb = device->fb.access_fb(device->ctx);
            
            uint8_t* dst_base = (uint8_t*)(fb.phys + etc::hhdm());
            uint8_t* src_base = (uint8_t*)(info->phys + etc::hhdm()); 

            uint64_t bpp = fb.bpp / 8;
            if (bpp == 0) bpp = 4;

            if (in->num_clips == 0 || in->clips_ptr == 0) {
                klibc::memcpy(dst_base, src_base, fb.height * fb.pitch);
            } else {
                
                auto clips = (drm_clip_rect*)in->clips_ptr;

                for (uint32_t i = 0; i < in->num_clips; ++i) {
                    drm_clip_rect clip = clips[i];

                    if (clip.x1 >= fb.width || clip.y1 >= fb.height) continue;
                    if (clip.x2 > fb.width)  clip.x2 = fb.width;
                    if (clip.y2 > fb.height) clip.y2 = fb.height;
                    if (clip.x1 >= clip.x2 || clip.y1 >= clip.y2) continue;

                    std::uint64_t bytes_to_copy = (std::uint64_t)(clip.x2 - clip.x1) * bpp;
                    std::uint64_t x_offset = (std::uint64_t)clip.x1 * bpp;

                    for (std::uint64_t y = clip.y1; y < clip.y2; ++y) {
                        std::uint64_t row_offset = (y * fb.pitch) + x_offset;
                        klibc::memcpy(dst_base + row_offset, src_base + row_offset, bytes_to_copy);
                    }
                }
            }

            return 0;

        }
        break;
    }


    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_GETENCODER: {
        
        if(device->type == drm::drm_type::framebuffer) {
            auto in = (drm::drm_structs::drm_mode_get_encoder*)arg;
            auto encoder = (std::uint32_t*)drm::_kms.get(in->encoder_id);

            if(encoder == nullptr)
                assert(0, "fff");

            if(*encoder != 0x0DDEAC0D)
                assert(0, "fff vvv");

            in->crtc_id = device->crtc_ptr[0];
            in->encoder_type = DRM_MODE_ENCODER_NONE;
            in->possible_crtcs = 1;
            in->possible_clones = 0;

            return 0;

        } else {
            assert(0, "assert(0, assert(0, assert(0, assert(0, cxsmczc))))")
        }

    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_OBJ_GETPROPERTIES: {
        auto in = (drm::drm_structs::drm_mode_obj_get_properties*)arg;

        switch(in->obj_type) {
        
        case DRM_MODE_OBJECT_CONNECTOR: {
            auto info = (drm::drm_structs::drm_mode_get_connector*)drm::_kms.get(in->obj_id);

            if(info == nullptr)
                assert(0,"conn fail %dP", in->obj_id);

            if((void*)in->prop_values_ptr != nullptr) {
                klibc::memcpy((void*)in->prop_values_ptr, (void*)info->prop_values_ptr, in->count_props * sizeof(std::uint64_t));
            }

            if((void*)in->props_ptr != nullptr) {
                klibc::memcpy((void*)in->props_ptr, (void*)info->props_ptr, in->count_props * sizeof(std::uint32_t));
            } else {
                in->count_props = info->count_props;
            }

            return 0;
            
        } 

        case DRM_MODE_OBJECT_CRTC: {
            if(device->type == drm::drm_type::framebuffer) {
                auto magic = (drm::drm_structs::generic_crtc*)drm::_kms.get(in->obj_id);
                
                if(magic == nullptr)
                    assert(0, "f");

                if(magic->magic != 0xDEAD1234)
                    assert(0, "v");

                if((void*)in->prop_values_ptr != nullptr) {
                    klibc::memcpy((void*)in->prop_values_ptr, (void*)magic->prop_values_ptr, in->count_props * sizeof(std::uint64_t));
                }

                if((void*)in->props_ptr != nullptr) {
                    klibc::memcpy((void*)in->props_ptr, (void*)magic->props_ptr, in->count_props * sizeof(std::uint32_t));
                } else {
                    in->count_props = magic->props_count;
                }

                return 0;
            }
        }

        case DRM_MODE_OBJECT_PLANE: {
            if(device->type == drm::drm_type::framebuffer) {
                auto info = (drm::drm_structs::drm_mode_get_plane*)drm::_kms.get(in->obj_id);

                if(info == nullptr)
                    assert(0, "n");

                if(info->magic != 0x8934AAFFBB91001B)
                    assert(0, "m");

                if((void*)in->prop_values_ptr != nullptr) {
                    device->plane_prop_values[5] = device->active_fb;
                    klibc::memcpy((void*)in->prop_values_ptr, (void*)device->plane_prop_values, in->count_props * sizeof(std::uint64_t));
                }

                if((void*)in->props_ptr != nullptr) {
                    klibc::memcpy((void*)in->props_ptr, (void*)device->plane_props, in->count_props * sizeof(std::uint32_t));
                } else {
                    in->count_props = device->plane_prop_count;
                }

                return 0;

            }
        }

        }

        assert(0, "getproperties type 0x%p, id 0x%p", in->obj_type, in->obj_id);
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_GETCRTC: {
        auto in = (drm::drm_structs::drm_mode_crtc*)arg;
        
        if(device->type == drm::drm_type::framebuffer) {
            auto magic = (drm::drm_structs::generic_crtc*)drm::_kms.get(in->crtc_id);
            
            if(magic == nullptr)
                assert(0, "f");

            if(magic->magic != 0xDEAD1234)
                assert(0, "v");

            in->fb_id = device->active_fb;
            in->mode = device->current_mode;
            in->mode_valid = 1;
            in->x = 0;
            in->y = 0;
            in->gamma_size = 0;

            if((void*)in->set_connectors_ptr != nullptr) {
                klibc::memcpy((void*)in->set_connectors_ptr, (void*)device->connector_ptr, in->count_connectors * sizeof(std::uint32_t));
            } else {
                in->count_connectors = device->connector_count;
            }

            return 0;
        }
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_GETPLANERESOURCES: {
        auto in = (drm::drm_structs::drm_mode_get_plane_res*)arg;

        if((void*)in->plane_id_ptr != nullptr) {
            klibc::memcpy((void*)in->plane_id_ptr, device->plane_ptr, in->count_planes * sizeof(std::uint32_t));
        } else {
            in->count_planes = device->plane_count;
        }

        return 0;
    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_GETPLANE: {
        auto in = (drm::drm_structs::drm_mode_get_plane*)arg;
        auto info = (drm::drm_structs::drm_mode_get_plane*)drm::_kms.get(in->plane_id);

        if(info == nullptr)
            assert(0, "n");

        if(info->magic != 0x8934AAFFBB91001B)
            assert(0, "m");

        in->fb_id = device->active_fb;
        in->crtc_id = info->crtc_id;
        in->possible_crtcs = info->possible_crtcs;
        in->gamma_size = 0;
        
        if((void*)in->format_type_ptr != nullptr) {
            klibc::memcpy((void*)in->format_type_ptr, (void*)info->format_type_ptr, sizeof(std::uint32_t) * in->count_format_types);
        } else {
            in->count_format_types = info->count_format_types;
        }

        return 0;

    }

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_CURSOR2:
    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_CURSOR:
        if(device->type == drm::drm_type::framebuffer)
            return -ENOTSUP; // simpledrm doesnt have something like this
        break;

    case drm::drm_structs::drm_requests::DRM_IOCTL_SET_MASTER:
        return 0; // todo 

    case drm::drm_structs::drm_requests::DRM_IOCTL_DROP_MASTER:
        return 0; // todo

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_LIST_LESSEES:
        if(device->type == drm::drm_type::framebuffer)
            return 0; // simpledrm doesnt have something like this
        break;

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_SETGAMMA:
        return -EINVAL;
    
    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_SETCRTC:
        return 0;

    case drm::drm_structs::drm_requests::DRM_IOCTL_MODE_SETPROPERTY: {
        auto in = (drm::drm_structs::drm_mode_connector_set_property*)arg;
        
        if(device->type == drm::drm_type::framebuffer) {
            auto info = (drm::drm_structs::drm_mode_get_connector*)drm::_kms.get(in->connector_id);

            if(info == nullptr)
                assert(0,"conn fail %dv", info->connector_id);

            auto props = (std::uint32_t*)info->props_ptr;

            for(std::uint32_t i = 0;i < info->count_props; i++) {
                if(props[i] == in->prop_id) {
                    auto prop = (drm::drm_structs::drm_mode_get_property*)drm::_kms.get(in->prop_id);
                    ((std::uint64_t*)info->prop_values_ptr)[i] = in->value;

                    log("drm", "setting property %s to %d", prop->name, in->value);
                }
            } 

            return 0;
        }

        assert(0, "setproperty connector id %d, prop id %d, value %lli", in->connector_id, in->prop_id, in->value);
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