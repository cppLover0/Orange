
#include <stdint.h>
#include <generic/VFS/devfs.hpp>
#include <generic/VFS/vfs.hpp>
#include <other/log.hpp>
#include <other/string.hpp>
#include <generic/limineA/limineinfo.hpp>
#include <config.hpp>
#include <generic/memory/pmm.hpp>
#include <generic/memory/heap.hpp>

devfs_dev_t* head_dev = 0;
devfs_dev_t* last_dev = 0;

devfs_dev_t* devfs_find_dev(const char* loc) {

    devfs_dev_t* dev = head_dev;
    while(dev) {

        if(!String::strcmp(loc,dev->loc)) {
            return dev;
        }

        dev = dev->next;
    }

    return 0;

}

int devfs_read(char* buffer,char* filename,long hint_size) {

    if(!filename) return 1;
    if(!buffer) return 2;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 3;
    if(!dev->read) return -15;

    return dev->read(buffer,hint_size);
}

int devfs_write(char* buffer,char* filename,uint64_t size,char is_symlink_path,uint64_t offset) {
    if(!filename) return 1;
    if(!buffer) return 2;
    if(!size) return 3;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->write) return -15;

    return dev->write(buffer,size,offset);

}

int devfs_ioctl(char* filename,unsigned long request, void *arg, int *result) {
    if(!filename) return 1;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->ioctl) return -15;

    return dev->ioctl(request,arg,(void*)result);

}


int devfs_askforpipe(char* filename,pipe_t* pipe) {

    if(!filename) return 1;

    if(!pipe) return 2;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->askforpipe) return -15;

    return dev->askforpipe(pipe);

}

int devfs_instantreadpipe(char* filename,pipe_t* pipe) {
    if(!filename) return 1;

    if(!pipe) return 2;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->instantreadpipe) return -15;

    return dev->instantreadpipe(pipe);
}

int devfs_stat(char* filename,char* buffer,char follow_symlinks) {
    if(!filename) return 1;

    if(!buffer) return 2;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->stat) return -15;

    return dev->stat(buffer);
}

void devfs_reg_device(const char* name,int (*write)(char* buffer,uint64_t size,uint64_t offset),int (*read)(char* buffer,long hint_size),int (*askforpipe)(pipe_t* pipe),int (*instantreadpipe)(pipe_t* pipe),int (*ioctl)(unsigned long request, void *arg, void* result)) {

    struct devfs_dev* dev = (devfs_dev_t*)PMM::VirtualAlloc();
    
    if(dev) {
        String::memset(dev,0,4096);
        String::memcpy(dev->loc,name,String::strlen((char*)name));
        dev->write = write;
        dev->read = read;
        dev->askforpipe = askforpipe;
        dev->instantreadpipe = instantreadpipe;
        dev->ioctl = ioctl;

        if(!head_dev) {
            head_dev = dev;
            last_dev = dev;
        } else {
            last_dev->next = dev;
            last_dev = dev;
        }

    } else {
        KHeap::Free(dev);
    }

}

void devfs_advanced_configure(const char* name,int (*stat)(char* buffer)) {
    devfs_dev_t* dev = devfs_find_dev(name);
    dev->stat = stat;
}

/*

Some basic devfs 

*/

int zero_read(char* buffer,long hint_size) {
    if(!hint_size) return 7;
    if(hint_size > TMPFS_MAX_SIZE) return 8;

    String::memset(buffer,0,hint_size < 0 ? hint_size * (-1) : hint_size);

    return 0;
}

int fbdev_write(char* buffer,uint64_t size,uint64_t offset) {
    LimineInfo info;
    uint64_t fb = (uint64_t)info.fb_info->address;
    fb += offset;
    if(size > (info.fb_info->width * info.fb_info->pitch))
        size = (info.fb_info->width * info.fb_info->pitch) - offset;
    String::memcpy((void*)fb,buffer,size);
    return 0;
}

int fbdev_ioctl(unsigned long request, void *arg, void* result) {
    LimineInfo info;
    switch(request) {
        case FBIOGET_VSCREENINFO: {
            fb_var_screeninfo fb;
            String::memset(&fb,0,sizeof(fb_var_screeninfo));
            fb.red.length = info.fb_info->red_mask_size;
            fb.red.offset = info.fb_info->red_mask_shift;
            fb.blue.offset = info.fb_info->blue_mask_shift;
            fb.blue.length = info.fb_info->blue_mask_size;
            fb.green.length = info.fb_info->green_mask_size;
            fb.green.offset = info.fb_info->green_mask_shift;
            fb.xres = info.fb_info->width;
            fb.yres = info.fb_info->height;
            fb.bits_per_pixel = info.fb_info->bpp < 5 ? (info.fb_info->bpp * 8) : info.fb_info->bpp;
            fb.xres_virtual = info.fb_info->width;
            fb.yres_virtual = info.fb_info->height;
            String::memcpy(arg,&fb,sizeof(fb_var_screeninfo));
            break;
        }
        case FBIOGET_FSCREENINFO: {
            fb_fix_screeninfo fb;
            String::memset(&fb,0,sizeof(fb_fix_screeninfo));
            fb.line_length = info.fb_info->pitch;
            fb.smem_len = info.fb_info->pitch * info.fb_info->width;
            String::memcpy(arg,&fb,sizeof(fb_fix_screeninfo));
        }
    }
    return 0;
}

int fbdev_stat(char* buffer) {
    filestat_t* stat = (filestat_t*)buffer;
    LimineInfo info;
    stat->content = (char*)info.fb_info->address;
    stat->size = info.fb_info->pitch * info.fb_info->width;
    stat->fs_prefix1 = 'D';
    stat->fs_prefix2 = 'E';
    stat->fs_prefix3 = 'V';
    return 0;
}

char __zero_null = 0;

int zero_stat(char* buffer) {
    filestat_t* stat = (filestat_t*)buffer;
    LimineInfo info;
    stat->content = (char*)&__zero_null;
    stat->size = 8;
    stat->fs_prefix1 = 'D';
    stat->fs_prefix2 = 'E';
    stat->fs_prefix3 = 'V';
    return 0;
}

void devfs_init(filesystem_t* fs) {
    devfs_reg_device("/zero",0,zero_read,0,0,0);
    devfs_reg_device("/null",0,zero_read,0,0,0);
    devfs_reg_device("/fb0",fbdev_write,0,0,0,fbdev_ioctl);
    devfs_advanced_configure("/fb0",fbdev_stat);
    devfs_advanced_configure("/zero",zero_stat);
    devfs_advanced_configure("/null",zero_stat);

    fs->create = 0;
    fs->disk = 0;
    fs->exists = 0;
    fs->is_in_ram = 1;
    fs->readfile = devfs_read;
    fs->writefile = devfs_write;
    fs->askforpipe = devfs_askforpipe;
    fs->ioctl = devfs_ioctl;
    fs->touch = 0;
    fs->stat = devfs_stat;
    fs->rm = 0;

}