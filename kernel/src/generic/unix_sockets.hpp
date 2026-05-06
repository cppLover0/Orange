#pragma once

#include <cstdint>
#include <generic/vfs.hpp>
#include <atomic>
#include <generic/lock/spinlock.hpp>
#include <generic/scheduling.hpp>
#include <utils/ringbuffer.hpp>

typedef unsigned short sa_family_t;

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

struct unix_socket_pending_connection {
    struct file_descriptor* file;
    locks::spinlock is_accepted;
    std::atomic<bool> is_used;
};

struct unix_socket_node {
    unix_socket_pending_connection pend_conns[64];
    sockaddr_un path;
    std::uint64_t mode;
    std::atomic<std::uint64_t> conn_counter;
    unix_socket_node* next;
};

static_assert(sizeof(unix_socket_node) < 4096, "13211415131");

namespace unix_sockets {
    bool is_exists(sockaddr_un* path, bool is_internal);
    long long bind(file_descriptor* file, sockaddr_un* path);
    long long connect(file_descriptor* file, sockaddr_un* path);
    long long accept(thread* proc, file_descriptor* file, sockaddr_un* path);

    unix_socket_node* find(sockaddr_un* path);
}