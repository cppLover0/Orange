#pragma once

#include <cstdint>
#include <generic/vfs.hpp>
#include <atomic>
#include <generic/lock/spinlock.hpp>
#include <generic/scheduling.hpp>
#include <utils/ringbuffer.hpp>
#include <generic/unix_sockets_extern.hpp>

static_assert(sizeof(unix_socket_node) < 4096, "13211415131");

namespace unix_sockets {
    bool is_exists(sockaddr_un* path, bool is_internal);
    long long bind(file_descriptor* file, sockaddr_un* path);
    long long connect(thread* proc, file_descriptor* file, sockaddr_un* path);
    long long accept(thread* proc, file_descriptor* file, sockaddr_un* path);
    
    unix_socket_node* find(sockaddr_un* path);
}