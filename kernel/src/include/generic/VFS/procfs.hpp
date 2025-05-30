
// i'll use it only for neofetch :^)

#pragma once

#include <generic/VFS/vfs.hpp>


typedef struct procfs_data {
    
    int (*read)(char* buffer,long hint_size);

    struct procfs_data* next;
    char loc[2048];

} __attribute__((packed)) procfs_data_t;

class ProcFS {
public:
    static void Init(filesystem_t* fs);
};