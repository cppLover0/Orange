#include <generic/keyboard.hpp>
#include <generic/devfs.hpp>
#include <cstdint>
#include <utils/ringbuffer.hpp>

utils::ring_buffer<std::uint8_t>* keys;

bool kbd_poll(file_descriptor* fd, devfs_node* node, vfs_poll_type type) {
    (void)node;
    if(type == vfs_poll_type::pollin) {
        return keys->is_not_empty(fd->other.queue, fd->other.cycle);
    } else if(type == vfs_poll_type::pollout) {
        return false;
    }
    return false;
}

std::int32_t kbd_open(file_descriptor* fd, devfs_node* node) {
    (void)fd;
    (void)node;
    fd->other.cycle = keys->cycle;
    fd->other.queue = keys->tail;
    return 0;
}

signed long kbd_read(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)node;
    return keys->receive((std::uint8_t*)buffer, count, &fd->other.cycle, &fd->other.queue);
}

void keyboard::init() {
    devfs::create(false, (char*)"/keyboard", nullptr, 0, 0, kbd_open, nullptr, kbd_read, nullptr, kbd_poll, nullptr, false, 0);
    keys = new utils::ring_buffer<std::uint8_t>(256);
}

void keyboard::submit(std::uint8_t key) {
    keys->send(key);
}
