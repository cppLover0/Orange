#include <generic/mouse.hpp>
#include <generic/devfs.hpp>
#include <cstdint>
#include <utils/ringbuffer.hpp>

utils::ring_buffer<mouse_packet_t>* mkeys;

bool mouse_poll(file_descriptor* fd, devfs_node* node, vfs_poll_type type) {
    (void)node;
    if(type == vfs_poll_type::pollin) {
        return mkeys->is_not_empty(fd->other.queue, fd->other.cycle);
    } else if(type == vfs_poll_type::pollout) {
        return false;
    }
    return false;
}

std::int32_t mouse_open(file_descriptor* fd, devfs_node* node) {
    (void)fd;
    (void)node;
    fd->other.cycle = mkeys->cycle;
    fd->other.queue = mkeys->tail;
    return 0;
}

signed long mouse_read(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)node;
    return mkeys->receive((mouse_packet_t*)buffer, count, &fd->other.cycle, &fd->other.queue);
}

void mouse::init() {
    devfs::create(false, (char*)"/mouse", nullptr, 0, 0, mouse_open, nullptr, mouse_read, nullptr, mouse_poll, nullptr, false, 0);
    mkeys = new utils::ring_buffer<mouse_packet_t>(256);
}

void mouse::submit(mouse_packet_t key) {
    mkeys->send(key);
}
