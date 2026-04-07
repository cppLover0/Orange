#include <generic/devfs.hpp>
#include <generic/fbdev.hpp>
#include <generic/bootloader/bootloader.hpp>
#include <utils/assert.hpp>
#include <utils/errno.hpp>
#include <generic/hhdm.hpp>
#include <generic/paging.hpp>
#include <utils/linux.hpp>
#include <klibc/string.hpp>
#include <generic/sysfs.hpp>
#include <atomic>

std::int32_t fbdev_ioctl(devfs_node* node, std::uint64_t req, void* arg) {
    fbdev::fbdev_arg* arg2 = (fbdev::fbdev_arg*)node->arg;
    switch(req) {
    case FBIOGET_VSCREENINFO:
        klibc::memcpy(arg, &arg2->var, sizeof(arg2->var));
        return 0;
    case FBIOGET_FSCREENINFO:
        klibc::memcpy(arg, &arg2->fix, sizeof(arg2->fix));
        return 0;
    }
    assert(0,"fbdev stfu");
    return -EINVAL;
}

void fbdev::init() {

    sysfs::create_dir("/class/graphics/fbcon");
    sysfs::create_symlink("/class/graphics/fbcon/subsystem", (char*)"../..");
    sysfs::create("/class/graphics/fbcon/rotate", (void*)"0\0", 2);
    sysfs::create("/class/graphics/fbcon/cursor_blink", (void*)"-1\0", 3);

    limine_framebuffer_response* fbdevs = bootloader::bootloader->get_framebuffers();
    for(std::uint64_t i = 0;i < fbdevs->framebuffer_count; i++) {
        log("fbdev", "found framebuffer %d %llix%llix%lli with base 0x%p",i, fbdevs->framebuffers[i]->width, fbdevs->framebuffers[i]->height, fbdevs->framebuffers[i]->bpp, fbdevs->framebuffers[i]->address);
        
        char buffer[256];
        klibc::__printfbuf(buffer, 256, "/fb%d", i);

        fbdev::fbdev_arg* new_arg = new fbdev::fbdev_arg;

        fb_fix_screeninfo* finfo = &new_arg->fix;
        fb_var_screeninfo* vinfo = &new_arg->var;

        limine_framebuffer fb = *fbdevs->framebuffers[i];

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

        sysfs::create_dir("/class/graphics/fb%d", i);
        sysfs::create_symlink("/class/graphics/fb%d/subsystem", (char*)"../..", i);
        sysfs::create("/class/graphics/fb%d/rotate", (void*)"0\0", 2, i);
        sysfs::create("/class/graphics/fb%d/name", (void*)"vesafb\0", sizeof("vesafb"), i);

        char buffer2[256];
        int len2 = klibc::__printfbuf(buffer2, 256, "U:%llix%llip-0", fb.width, fb.height);
        sysfs::create("/class/graphics/fb%d/modes", (void*)buffer2, len2 + 1, i);
        sysfs::create("/class/graphics/fb%d/mode", (void*)buffer2, len2 + 1, i);

        sysfs::create("/class/graphics/fb%d/pan", (void*)("0,0\0"), sizeof("0,0"), i);

        len2 = klibc::__printfbuf(buffer2, 256, "%lli,%lli", fb.width, fb.height);
        sysfs::create("/class/graphics/fb%d/virtual_size", buffer2, len2, i);

        len2 = klibc::__printfbuf(buffer2, 256, "%d", vinfo->bits_per_pixel);
        sysfs::create("/class/graphics/fb%d/bits_per_pixel", (void*)buffer2, len2 + 1, i);
        sysfs::create("/class/graphics/fb%d/blank", (void*)"0\0", sizeof("0"), i);
        sysfs::create("/class/graphics/fb%d/state", (void*)"0\0", sizeof("0"), i);

        len2 = klibc::__printfbuf(buffer2, 256, "29:%d", i);
        sysfs::create("/class/graphics/fb%d/dev", buffer2, len2, i);

        sysfs::create("/class/graphics/fb%d/console", (void*)"\0", 1, i);
        sysfs::create("/class/graphics/fb%d/cursor", (void*)"\0", 1, i);

        devfs::create(false, buffer, new_arg, (std::uint64_t)fb.address - etc::hhdm(), fb.pitch * fb.height, nullptr, fbdev_ioctl, nullptr, nullptr, nullptr, nullptr, false, PAGING_WC);
    }
}
