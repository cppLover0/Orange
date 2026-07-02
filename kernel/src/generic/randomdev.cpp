#include <generic/devfs.hpp>
#include <cstdint>
#include <generic/randomdev.hpp>
#include <utils/random.hpp>

std::int32_t randomdev_open(file_descriptor* fd, devfs_node* node) {
    (void)fd;
    (void)node;
    return 0;
}

signed long randomdev_read(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)fd;
    (void)node;
    
    for(std::size_t i = 0; i < count; i++) {
        ((char*)buffer)[i] = random::random() & 0xFF;
    }
    
    return count;
}

signed long randomdev_write(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)fd;
    (void)node;
    (void)buffer;
    return count;
}

bool randomdev_poll(file_descriptor* fd, devfs_node* node, vfs_poll_type type) {
    (void)fd;
    (void)node;

    if(type == vfs_poll_type::pollin)
        return true;
    return false;
}

void randomdev::init() {
    devfs::create(false, (char*)"/random", nullptr, 0, 0, randomdev_open, nullptr, randomdev_read, randomdev_write, randomdev_poll,  nullptr, false, 0);
    devfs::create(false, (char*)"/urandom", nullptr, 0, 0, randomdev_open, nullptr, randomdev_read, randomdev_write, randomdev_poll,  nullptr, false, 0);
}

