
#include <cstdint>
#include <generic/vfs/vfs.hpp>
#include <etc/list.hpp>

#pragma once

#define DEVFS_PACKET_CREATE_DEV 1
#define DEVFS_PACKET_READ_READRING 2
#define DEVFS_PACKET_WRITE_READRING 3 
#define DEVFS_PACKET_READ_WRITERING 4 
#define DEVFS_PACKET_WRITE_WRITERING 5
#define DEVFS_PACKET_CREATE_IOCTL 6 
#define DEVFS_PACKET_SIZE_IOCTL 7
#define DEVFS_PACKET_IOCTL 8
#define DEVFS_ENABLE_PIPE 9
#define DEVFS_SETUP_MMAP 10
#define DEVFS_PACKET_CREATE_PIPE_DEV 11
#define DEVFS_PACKET_ISATTY 12 
#define DEVFS_PACKET_SETUPTTY 13
#define DEVFS_GETSLAVE_BY_MASTER 14

struct	winsize {
 	unsigned short	 	ws_row;	 
 	unsigned short	 	ws_col;	 
 	unsigned short	 	ws_xpixel;	
/* unused */
 	unsigned short	 	ws_ypixel;	
/* unused */
};

#define TCGETS                   0x5401
#define TCSETS                   0x5402
#define TIOCGWINSZ               0x5413
#define TIOCSWINSZ               0x5414

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS     32

typedef struct {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
	speed_t ibaud;
	speed_t obaud;
} __attribute__((packed)) termios_t;

namespace vfs {

    typedef struct {
        std::uint64_t is_pipe : 1;
        std::uint64_t is_pipe_rw : 1;
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
                std::uint64_t flags;
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
        union {
            struct {
                Lists::Ring* readring;
                Lists::Ring* writering;
            };
            struct {
                pipe* readpipe;
                pipe* writepipe;
            };
        };
        devfs_ioctl_packet_t ioctls[32];
        std::uint64_t pipe0;
        std::uint64_t mmap_base;
        std::uint64_t mmap_size;
        std::uint64_t mmap_flags;
        std::int32_t dev_num;
        std::int8_t is_tty;

        std::int64_t (*read)(userspace_fd_t* fd, void* buffer, std::uint64_t count);
        std::int64_t (*write)(userspace_fd_t* fd, void* buffer, std::uint64_t size);
        std::int32_t (*ioctl)(userspace_fd_t* fd, unsigned long req, void *arg, int *res);
        std::int32_t (*open)(userspace_fd_t* fd, char* path);

        struct devfs_node* next;
        char masterpath[256];
        char slavepath[256];
    } devfs_node_t;

    static_assert(sizeof(devfs_node_t) < 4096,"devfs_node_t is higher than fucking page size");

    class devfs {
    public:
        static void mount(vfs_node_t* node);
        static std::int64_t send_packet(char* path,devfs_packet_t* packet);
    };
};
