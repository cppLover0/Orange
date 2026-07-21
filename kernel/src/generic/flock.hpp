#pragma once

#include <cstdint>
#include <generic/heap.hpp>

#define F_RDLCK         0
#define F_WRLCK         1
#define F_UNLCK         2


namespace flock {

    struct flock_struct {
        short l_type;
        short l_whence;
        std::int64_t l_start;
        std::int64_t l_len;
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

    flock_list* access_source(void* fs1, int inode);

    flock_struct* create(void* fs1, int inode, std::int64_t start, std::int64_t len, short lock, short whence, std::int64_t seek, std::size_t file_size, int pid);
    flock_struct* search(void* fs1, int inode, std::int64_t start, std::int64_t len, short whence, std::int64_t seek, std::size_t file_size);
    void remove(void* fs1, int inode);
    
};