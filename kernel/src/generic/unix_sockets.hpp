#pragma once

#include <cstdint>
#include <generic/vfs.hpp>
#include <atomic>

typedef unsigned short sa_family_t;

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

struct unix_socket_pending_connection {
    struct file_descriptor* file;
    std::atomic<bool> is_used;
};

struct unix_socket_node {
    unix_socket_pending_connection pend_conns[64];
    sockaddr_un path;
    unix_socket_node* next;
};

namespace unix_sockets {
    bool is_exists(sockaddr_un* path);
    long long bind(file_descriptor* file, sockaddr_un* path);
    long long connect(file_descriptor* file, sockaddr_un* path);
    long long accept(file_descriptor* file, sockaddr_un* path);

    unix_socket_node* find(sockaddr_un* path);
}