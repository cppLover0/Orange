#pragma once

#include <atomic>
#include <generic/lock/spinlock.hpp>
#include <generic/scheduling.hpp>

typedef unsigned short sa_family_t;

struct un_ucred {
    int pid; 
    int uid;  
    int gid; 
};

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

struct unix_socket_pending_connection {
    void* file;
    thread* proc;
    locks::spinlock is_accepted;
    std::atomic<bool> is_used;
};

struct unix_socket_node {
    unix_socket_pending_connection pend_conns[64];
    sockaddr_un path;
    std::uint64_t mode;
    std::atomic<std::uint64_t> conn_counter;
    std::atomic<std::uint64_t> link_counter;
    std::atomic<std::uint64_t> open_counter;

    unix_socket_node* next;
};

static_assert(sizeof(unix_socket_node) < 4096, "meow. unix");

extern "C" void process_close(void* node1);


