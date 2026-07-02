#pragma once

#include <cstdint>
#include <generic/vfs.hpp>
#include <generic/heap.hpp>

#define F_RDLCK         0
#define F_WRLCK         1
#define F_UNLCK         2


namespace flock {

    struct flock_struct {
        short l_type;
        short l_whence;
        off_t l_start;
        off_t l_len;
        int l_pid;
    };

    struct flock_list {
        bool is_used;
        int inode;
        std::size_t ptr;
        flock_struct* lock;
    };

    struct flock_high {
        flock_list* flocks;
    };

    flock_struct* create(filesystem* fs, int inode, off_t start, off_t len, short lock, short whence, off_t seek, std::size_t file_size, int pid);
    flock_struct* search(filesystem* fs, int inode, off_t start, off_t len, short whence, off_t seek, std::size_t file_size);
    void remove(filesystem* fs, int inode);
    
};