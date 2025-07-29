
#include <cstdint>
#include <generic/vfs/vfs.hpp>
#include <etc/list.hpp>

#pragma once

#define DEVFS_PACKET_CREATE_DEV 1
#define DEVFS_PACKET_READ_READRING 2
#define DEVFS_PACKET_WRITE_READRING 3 /* Will be used from some syscall (non posix) */
#define DEVFS_PACKET_READ_WRITERING 4 /* Will be used from some syscall (non posix) */
#define DEVFS_PACKET_WRITE_WRITERING 5
#define DEVFS_PACKET_CREATE_IOCTL 6 
#define DEVFS_PACKET_SIZE_IOCTL 7
#define DEVFS_PACKET_IOCTL 8
#define DEVFS_ENABLE_PIPE 9
#define DEVFS_SETUP_MMAP 10
#define DEVFS_NONBLOCK_PIPE_OFFSET_IN_IOCTL 11

namespace vfs {

    typedef struct {
        std::uint64_t is_pipe : 1;
    } __attribute__((packed)) devfs_pipe_p_t;

    typedef struct {
        union {
            struct {
                std::uint8_t request;
                std::uint8_t* cycle;
                std::uint32_t* queue;
                std::uint32_t size;
                std::uint64_t value;
            };
            struct {
                std::uint8_t request;
                std::uint64_t ioctlreq;
                std::uint64_t arg;
            } ioctl;
            struct {
                std::uint8_t request;
                std::uint32_t writereg;
                std::uint32_t readreg;
                std::uint32_t size;
                std::uint64_t pointer;
            } create_ioctl;
            struct {
                std::uint8_t request;
                std::uint8_t pipe_target;
                std::uint64_t pipe_pointer;
            } enable_pipe;
            struct {
                std::uint8_t request;
                std::uint64_t dma_addr;
                std::uint64_t size;
            } setup_mmap;
        };
    } __attribute__((packed)) devfs_packet_t; /* User-Kernel interaction packet */

    typedef struct {
        std::uint32_t read_req;
        std::uint32_t write_req;
        std::uint32_t size;
        void* pointer_to_struct;
    } __attribute__((packed)) devfs_ioctl_packet_t;

    typedef struct devfs_node {
        devfs_pipe_p_t open_flags;
        Lists::Ring* readring;
        Lists::Ring* writering;
        devfs_ioctl_packet_t ioctls[32];
        std::uint64_t pipes[32];
        std::uint64_t mmap_base;
        std::uint64_t mmap_size;
        struct devfs_node* next;
        char path[512];
    } devfs_node_t;

    static_assert(sizeof(devfs_node_t) < 4096,"devfs_node_t is higher than fucking page size");

    class devfs {
    public:
        static void mount(vfs_node_t* node);
        static std::int32_t send_packet(char* path,devfs_packet_t* packet);
    };
};
