#include <generic/devfs.hpp>
#include <cstdint>
#include <generic/dev_log.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/drivers/serial.hpp>
#endif

std::int32_t log_open(file_descriptor* fd, devfs_node* node) {
    (void)fd;
    (void)node;
    return 0;
}

signed long log_read(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)fd;
    (void)node;
    (void)buffer;
    (void)count;
    return 0;
}

signed long log_write(file_descriptor* fd, devfs_node* node, void* buffer, std::size_t count) {
    (void)fd;
    (void)node;
    (void)buffer;
#if defined(__x86_64__)
    x86_64::serial::write_data((char*)buffer, count);
#endif
    return count;
}

void logdev::init() {
    devfs::create(false, (char*)"/serial", nullptr, 0, 0, log_open, nullptr, log_read, log_write, nullptr,  nullptr, false, 0);
}

