#pragma once
#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/lock/mutex.hpp>
#include <utils/linux.hpp>

enum class file_descriptor_type : std::uint8_t {
    unallocated = 0,
    file = 1,
    pipe = 2,
    socket = 3,
    epoll = 4,
    memfd = 5
};

struct stat {
    dev_t     st_dev;       
    ino_t     st_ino;         
    mode_t    st_mode;   
    nlink_t   st_nlink;       
    uid_t     st_uid;      
    gid_t     st_gid;        
    dev_t     st_rdev;      
    off_t     st_size;       
    blksize_t st_blksize;    
    blkcnt_t  st_blocks;     

    struct timespec st_atim;
    struct timespec st_mtim; 
    struct timespec st_ctim;  

#define st_atime st_atim.tv_sec    
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
};

struct filesystem { 
    locks::mutex lock;

    std::uint32_t (*open)(void* file_desc, char* path);
    
    char path[2048];
};

struct file_descriptor {
    
    file_descriptor_type type;
    std::uint32_t flags;

    union fs_specific {
        std::uint64_t ino;
        std::uint64_t tmpfs_pointer;
    };

    struct vnode {
        disk* target_disk;
        filesystem* fs;

        signed long (*read)(file_descriptor* file, void* buffer, signed long count);
        signed long (*write)(file_descriptor* file, void* buffer, signed long count);
        signed long (*stat)(file_descriptor* file, stat* out);
        void (*close)(file_descriptor* file);
    };

};

namespace vfs {

    void init();
    void open(file_descriptor* fd, char* path, bool follow_symlinks );
    void create(char* path, std::uint32_t mode); // mode contains type too
}

// fs should setup this during open 
