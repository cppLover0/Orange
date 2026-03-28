#pragma once
#include <cstdint>
#include <generic/vfs.hpp>

struct devfs_node {
    vfs::pipe* pipe;
};

// for non input devices
namespace devfs {
    void create();
    void init(vfs::node* new_node);
}