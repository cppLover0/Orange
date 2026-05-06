#include <generic/devfs.hpp>
#include <cstdint>
#include <generic/null.hpp>

std::int32_t null_open(file_descriptor* fd, devfs_node* node) {
    (void)fd;
    (void)node;
    return 0;
}

signed long null_read(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)fd;
    (void)node;
    (void)buffer;
    (void)count;
    return 0;
}

signed long null_write(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)fd;
    (void)node;
    (void)buffer;
    return count;
}

void nulldev::init() {
    devfs::create(false, (char*)"/null", nullptr, 0, 0, null_open, nullptr, null_read, null_write, nullptr,  nullptr, false, 0);
}

