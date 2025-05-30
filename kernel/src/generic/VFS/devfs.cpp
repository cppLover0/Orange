
#include <stdint.h>
#include <generic/VFS/devfs.hpp>
#include <generic/VFS/vfs.hpp>
#include <other/log.hpp>
#include <other/string.hpp>
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
    if(!dev->read) return 4;

    return dev->read(buffer,hint_size);
}

int devfs_write(char* buffer,char* filename,uint64_t size,char is_symlink_path,uint64_t offset) {
    if(!filename) return 1;
    if(!buffer) return 2;
    if(!size) return 3;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->write) return 5;

    return dev->write(buffer,size);

}

int devfs_ioctl(char* filename,unsigned long request, void *arg, int *result) {
    if(!filename) return 1;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->ioctl) return 0;

    return dev->ioctl(request,arg,(void*)result);

}


int devfs_askforpipe(char* filename,pipe_t* pipe) {

    if(!filename) return 1;

    if(!pipe) return 2;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->askforpipe) return 5;

    return dev->askforpipe(pipe);

}

int devfs_instantreadpipe(char* filename,pipe_t* pipe) {
    if(!filename) return 1;

    if(!pipe) return 2;

    devfs_dev_t* dev = devfs_find_dev(filename);

    if(!dev) return 4;
    if(!dev->instantreadpipe) return 5;

    return dev->instantreadpipe(pipe);
}

void devfs_reg_device(const char* name,int (*write)(char* buffer,uint64_t size),int (*read)(char* buffer,long hint_size),int (*askforpipe)(pipe_t* pipe),int (*instantreadpipe)(pipe_t* pipe),int (*ioctl)(unsigned long request, void *arg, void* result)) {

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

/*

Some basic devfs 

*/

int zero_read(char* buffer,long hint_size) {
    if(!hint_size) return 7;
    if(hint_size > TMPFS_MAX_SIZE) return 8;

    String::memset(buffer,0,hint_size < 0 ? hint_size * (-1) : hint_size);

    return 0;

}

void devfs_init(filesystem_t* fs) {
    devfs_reg_device("/zero",0,zero_read,0,0,0);
    devfs_reg_device("/null",0,zero_read,0,0,0);

    fs->create = 0;
    fs->disk = 0;
    fs->exists = 0;
    fs->is_in_ram = 1;
    fs->readfile = devfs_read;
    fs->writefile = devfs_write;
    fs->askforpipe = devfs_askforpipe;
    fs->ioctl = devfs_ioctl;
    fs->touch = 0;
    fs->stat = 0;
    fs->rm = 0;

}