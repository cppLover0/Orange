#pragma once
#include <cstdint>
#include <generic/vfs.hpp>

namespace tmpfs {

    struct tmpfs_node {
        union {
            char* content;
            tmpfs_node** directory_content;
        };
        vfs_file_type type;
        std::size_t size;
        std::size_t physical_size;
        std::uint64_t ino;
        std::uint64_t mode;
        char name[256];
    };

    void init_default(vfs::node* node);
}