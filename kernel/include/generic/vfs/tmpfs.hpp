
#include <generic/vfs/vfs.hpp>
#include <cstdint>

#pragma once

#define TMPFS_TYPE_NONE 0
#define TMPFS_TYPE_FILE 1
#define TMPFS_TYPE_DIRECTORY 2
#define TMPFS_TYPE_SYMLINK 3

namespace vfs {

    typedef struct tmpfs_node {
        std::uint64_t size;
        std::uint64_t busy;
        std::uint64_t vars[8];
        std::uint8_t type;
        std::uint8_t* content;
        std::uint64_t real_size; // real size in ram (optimization)
        struct tmpfs_node* next;
        char name[2048];
    } tmpfs_node_t;

    class tmpfs {
    public:
        static void mount(vfs_node_t* node);
    };
};

#define NODE_POOL_BLOCK_SIZE (16 * 1024 * 1024)

struct NodePoolBlock {
    vfs::tmpfs_node_t* block;
    NodePoolBlock* next;
};
