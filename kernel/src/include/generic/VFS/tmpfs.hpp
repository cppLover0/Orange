
#include <generic/VFS/vfs.hpp>
#include <generic/memory/heap.hpp>
#include <generic/memory/pmm.hpp>
#include <stdint.h>

#pragma once

#define TMPFS_TYPE_FILE 0
#define TMPFS_TYPE_DIRECTORY 1
#define TMPFS_TYPE_SYMLINK 2

typedef struct data_file {
    char type;
    char* name;
    char* content;
    char protection;
    uint32_t size_of_content;
    uint64_t file_create_date;
    uint64_t file_change_date;
    struct data_file* next;
    struct data_file* parent;
} __attribute__((packed)) data_file_t;

void tmpfs_dump();

class TMPFS {
public:
    static void Init(filesystem_t* fs);
};