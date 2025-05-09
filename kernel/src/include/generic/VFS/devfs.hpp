
#include <stdint.h>
#include <generic/VFS/vfs.hpp>

typedef struct devfs_dev {
    
    int (*write)(char* buffer,uint64_t size);
    int (*read)(char* buffer,long hint_size);
    int (*askforpipe)(pipe_t* pipe);
    int (*instantreadpipe)(pipe_t* pipe);

    char* loc;
    struct devfs_dev* next;

} __attribute__((packed)) devfs_dev_t;

void devfs_reg_device(const char* name,int (*write)(char* buffer,uint64_t size),int (*read)(char* buffer,long hint_size),int (*askforpipe)(pipe_t* pipe),int (*instantreadpipe)(pipe_t* pipe));

void devfs_init(filesystem_t* fs);