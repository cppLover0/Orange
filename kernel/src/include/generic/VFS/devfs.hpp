
#include <stdint.h>
#include <generic/VFS/vfs.hpp>

#pragma once

typedef struct devfs_dev {
    
    int (*write)(char* buffer,uint64_t size);
    int (*read)(char* buffer,long hint_size);
    int (*askforpipe)(pipe_t* pipe);
    int (*instantreadpipe)(pipe_t* pipe);
    int (*ioctl)(unsigned long request, void* arg, void* result);

    struct devfs_dev* next;

    char loc[2048];
    

} __attribute__((packed)) devfs_dev_t;

void devfs_reg_device(const char* name,int (*write)(char* buffer,uint64_t size),int (*read)(char* buffer,long hint_size),int (*askforpipe)(pipe_t* pipe),int (*instantreadpipe)(pipe_t* pipe),int (*ioctl)(unsigned long request, void* arg, void* result));

void devfs_init(filesystem_t* fs);