#pragma once
#include <cstdint>
#include <generic/vfs.hpp>

struct devfs_node {
    vfs::pipe* write_pipe;
    vfs::pipe* read_pipe;

    std::int32_t (*open)(file_descriptor* fd, devfs_node* node);
    std::int32_t (*ioctl)(devfs_node* node, std::uint64_t req, void* arg);
    signed long (*read)(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count);
    signed long (*write)(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count);
    bool (*poll)(file_descriptor* fd, devfs_node* node, vfs_poll_type type);

    std::int32_t (*close)(file_descriptor* fd, devfs_node* node);

    void* arg;

    bool is_root;
    bool is_directory;

    bool is_a_tty;
    char path[256];

    std::uint64_t mmap;
    std::uint64_t mmap_size;
    std::uint64_t mmap_flags;

    std::uint64_t id;

    devfs_node* next;
};

static_assert(sizeof(devfs_node) < 4096, "fsfsdf");

// for non input devices
namespace devfs {
    devfs_node* create(bool is_tty, char* path, void* arg, std::uint64_t mmap, std::uint64_t mmap_size, std::int32_t (*open)(file_descriptor*fd, devfs_node* node), std::int32_t (*ioctl)(devfs_node* node, std::uint64_t req, void* arg), signed long (*read)(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count), signed long (*write)(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count), bool (*poll)(file_descriptor* fd, devfs_node* node, vfs_poll_type type), std::int32_t (*close)(file_descriptor* fd, devfs_node* node), bool is_directory = false, std::uint64_t mmap_flags = 0);
    void init(vfs::node* new_node);
}