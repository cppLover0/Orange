
// todo: do procfs

#pragma once
#include <cstdint>
#include <generic/vfs.hpp>

namespace procfs {

    struct procfs_action {

    };

    struct procfs_node {
        std::uint64_t tid;
        procfs_node* next;
    };

    void init(vfs::node* node);
};