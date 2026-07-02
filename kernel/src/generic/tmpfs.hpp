#pragma once
#include <cstdint>
#include <generic/vfs.hpp>

namespace tmpfs {

    struct tmpfs_node;

    struct directory_cont {
        char name[256];
        tmpfs_node* node;
    };

    struct tmpfs_node {
        union {
            char* content;
            directory_cont* dirents;
        };
        vfs_file_type type;
        std::uint64_t busy_counter;
        std::uint64_t nlink;
        std::size_t size;
        std::size_t physical_size;
        std::uint64_t ino;
        std::uint64_t mode;
        std::uint64_t create_time;
        std::uint64_t modify_time;
        std::uint64_t access_time;

        int uid;
        int gid;
    
        bool should_unlink;
    };

    struct legacy_tmpfs_node {
        union {
            char* content;
            legacy_tmpfs_node** directory_content;
        };
        vfs_file_type type;
        std::uint64_t busy_counter;
        std::size_t size;
        std::size_t physical_size;
        std::uint64_t ino;
        std::uint64_t mode;
        std::uint64_t create_time;
        std::uint64_t modify_time;
        std::uint64_t access_time;
        bool should_unlink;
        char name[256];
    };

    void init_default(vfs::node* node);
}

std::int32_t tmpfs_opt_create(char* path, vfs_file_type type, std::uint32_t mode, char* content, std::uint64_t size, tmpfs::tmpfs_node** out);