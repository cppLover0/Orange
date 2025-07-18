
#include <cstdint>
#include <atomic>

#pragma once

#include <config.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/locks/spinlock.hpp>
#include <etc/libc.hpp>

#define USERSPACE_FD_STATE_UNUSED 0
#define USERSPACE_FD_STATE_FILE 1
#define USERSPACE_FD_STATE_PIPE 2

#define VFS_TYPE_NONE 0
#define VFS_TYPE_FILE 1
#define VFS_TYPE_DIRECTORY 2
#define VFS_TYPE_SYMLINK 3

/* open/fcntl.  */
#define O_ACCMODE	   0003
#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_RDWR		     02
#ifndef O_CREAT
# define O_CREAT	   0100	/* Not fcntl.  */
#endif
#ifndef O_EXCL
# define O_EXCL		   0200	/* Not fcntl.  */
#endif
#ifndef O_NOCTTY
# define O_NOCTTY	   0400	/* Not fcntl.  */
#endif
#ifndef O_TRUNC
# define O_TRUNC	  01000	/* Not fcntl.  */
#endif
#ifndef O_APPEND
# define O_APPEND	  02000
#endif
#ifndef O_NONBLOCK
# define O_NONBLOCK	  04000
#endif
#ifndef O_NDELAY
# define O_NDELAY	O_NONBLOCK
#endif
#ifndef O_SYNC
# define O_SYNC	       04010000
#endif
#define O_FSYNC		O_SYNC
#ifndef O_ASYNC
# define O_ASYNC	 020000
#endif
#ifndef __O_LARGEFILE
# define __O_LARGEFILE	0100000
#endif

#ifndef __O_DIRECTORY
# define __O_DIRECTORY	0200000
#endif
#ifndef __O_NOFOLLOW
# define __O_NOFOLLOW	0400000
#endif
#ifndef __O_CLOEXEC
# define __O_CLOEXEC   02000000
#endif
#ifndef __O_DIRECT
# define __O_DIRECT	 040000
#endif
#ifndef __O_NOATIME
# define __O_NOATIME   01000000
#endif
#ifndef __O_PATH
# define __O_PATH     010000000
#endif
#ifndef __O_DSYNC
# define __O_DSYNC	 010000
#endif
#ifndef __O_TMPFILE
# define __O_TMPFILE   (020000000 | __O_DIRECTORY)
#endif

#define PIPE_SIDE_WRITE 1
#define PIPE_SIDE_READ 2

namespace vfs {

    class pipe {
    private:
        char* buffer;
        std::uint32_t connected_to_pipe = 0;
        std::uint32_t connected_to_pipe_write = 0;
        std::uint64_t size = 0;
        std::uint64_t total_size = 0;
        std::uint64_t flags = 0;
        locks::spinlock lock;
        std::atomic_flag is_received = ATOMIC_FLAG_INIT;
        std::atomic_flag is_n_closed = ATOMIC_FLAG_INIT;
        std::atomic_flag is_closed = ATOMIC_FLAG_INIT;
    public:

        pipe(std::uint64_t flags) {
            this->buffer = (char*)memory::pmm::_virtual::alloc(USERSPACE_PIPE_SIZE);
            this->total_size = USERSPACE_PIPE_SIZE;
            this->connected_to_pipe = 2; /* Syscall which creates pipe should create 2 fds too */ 
            this->connected_to_pipe_write = 1;
            this->flags = flags;
        }

        void close(std::uint8_t side) {
            this->lock.lock();
            this->connected_to_pipe--;
            
            if(side == PIPE_SIDE_WRITE) {
                this->connected_to_pipe_write--;
                if(this->connected_to_pipe_write == 0) {
                    this->is_received.clear();
                    this->is_n_closed.test_and_set();
                }
            }

            if(this->connected_to_pipe == 0) {
                delete this;
            }
            this->lock.unlock();
        }

        void create() {
            this->lock.lock();
            this->connected_to_pipe++;
            this->lock.unlock();
        }

        std::uint64_t force_write(char* buffer,std::uint64_t count) {
            char* temp_buffer = this->buffer;
            temp_buffer += size;
            memcpy(temp_buffer,buffer,count);
            this->size += count;
            return count;
        }

        std::uint64_t write(char* buffer,std::uint64_t count) {
            this->lock.lock();
            if(count >= (total_size - size)) {
                // wait until pipe can be free
                std::int64_t temp_size = count;
                while(temp_size > 0) {
                    std::uint64_t block_size = temp_size > total_size ? total_size : temp_size;
                    this->lock.unlock();
                    asm volatile("pause");
                    this->lock.lock();
                    if(this->is_received.test()) {
                        char* poop = buffer;
                        poop += (temp_size - count);
                        force_write(poop,block_size);
                        this->is_received.clear();
                    }
                    temp_size -= block_size;
                }
                return count;
            }
            force_write(this->buffer + this->size,count);
            return count;
            this->lock.unlock();
        }

        std::uint64_t read(char* buffer,std::uint64_t count) {
            if(this->is_closed.test()) {
                this->lock.unlock();
                return 0;
            }

            if(flags & O_NONBLOCK) {
                if(this->is_received.test()) {
                    this->lock.unlock();
                    return 0;
                }
            } else {
                while(this->is_received.test()) {
                    this->lock.unlock();
                    asm volatile("pause");
                    this->lock.lock();
                }
            }

            if(this->size == 0) {
                this->is_received.test_and_set();
                this->lock.unlock();
                return 0;
            }

            if(count >= this->size) {
                memcpy(buffer, this->buffer, this->size);
                std::uint64_t temp_size = this->size;
                this->size = 0;
                if(this->is_n_closed.test()) {
                    this->is_closed.test_and_set();
                }
                this->is_received.test_and_set();
                this->lock.unlock();
                return temp_size;
            }

            if(count < this->size) {
                std::uint64_t block_size = count;
                memcpy(buffer, this->buffer, block_size);
                memcpy(this->buffer, this->buffer + block_size, this->size - block_size);
                this->size -= block_size;
                this->lock.unlock();
                return block_size;
            }

            this->lock.unlock();
            return 0;
        }

        ~pipe() {
            memory::pmm::_virtual::free(this->buffer);
        }

    };

};

/* It should be restored in dup2 syscall when oldfd is 0 */
typedef struct {
    std::int32_t index;
    std::uint8_t state;
    std::uint8_t pipe_side;
    vfs::pipe* pipe;
} userspace_old_fd_t;

typedef struct {
    std::uint64_t offset;
    std::int32_t index;
    std::uint8_t state;
    std::uint8_t pipe_side;
    vfs::pipe* pipe;
    char path[2048];

    userspace_old_fd_t old_state;

} userspace_fd_t;

namespace vfs {
    class fd {
    public:
        /* Just helper function for non userspace usage */
        static inline void fill(userspace_fd_t* fd,char* name) {
            memcpy(fd->path,name,strlen(name));
        }
    };
};

#define __MLIBC_DIRENT_BODY std::uint64_t d_ino; \
			std::uint64_t d_off; \
			std::uint16_t d_reclen; \
			std::uint8_t d_type; \
			char d_name[1024];

#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_FOLLOW 0x400
#define AT_EACCESS 0x200
#define AT_NO_AUTOMOUNT 0x800
#define AT_EMPTY_PATH 0x1000

namespace vfs {

    typedef struct {
        __MLIBC_DIRENT_BODY;
    } dirent_t;

    typedef struct {
        unsigned long st_dev;
        unsigned long st_ino;
        unsigned long st_nlink;
        unsigned int st_mode;
        unsigned int st_uid;
        unsigned int st_gid;
        unsigned int __pad0;
        unsigned long st_rdev;
        long st_size;
        long st_blksize;
        long st_blocks;

    } __attribute__((packed)) stat_t;

    typedef struct {

        /* yes fd contain only full path, but char* path is relative to fs path, for example full path /dev/hi/l will be /hi/l */
        std::int32_t (*open)   (userspace_fd_t* fd, char* path                                            ); /* its used if fs decide to change some fd flags */
        std::int64_t (*write)  (userspace_fd_t* fd, char* path, void* buffer, std::uint64_t size          );
        std::int64_t (*read)   (userspace_fd_t* fd, char* path, void* buffer, std::uint64_t count         );
        std::int32_t (*stat)   (userspace_fd_t* fd, char* path, stat_t* out                               );
        std::int32_t (*var)    (userspace_fd_t* fd, char* path, std::uint64_t value, std::uint8_t request );
        std::int32_t (*ls)     (userspace_fd_t* fd, char* path, dirent_t* out                             ); 
        std::int32_t (*remove) (userspace_fd_t* fd, char* path                                            );
        std::int32_t (*chmod)  (userspace_fd_t* fd, char* path                                            );
        std::int32_t (*create) (char* path, std::uint8_t type                                             );
        std::int32_t (*touch)  (char* path                                                                );

        char path[2048];
    } vfs_node_t;

    class vfs {
    public:
        static void init();
        static std::int32_t open   (userspace_fd_t* fd                                            ); 
        static std::int64_t write  (userspace_fd_t* fd, void* buffer, std::uint64_t size          );
        static std::int64_t read   (userspace_fd_t* fd, void* buffer, std::uint64_t count         );
        static std::int32_t stat   (userspace_fd_t* fd, stat_t* out                               );
        static std::int32_t var    (userspace_fd_t* fd, std::uint64_t value, std::uint8_t request );
        static std::int32_t ls     (userspace_fd_t* fd, dirent_t* out                             ); 
        static std::int32_t remove (userspace_fd_t* fd                                            );
        static std::int32_t chmod  (userspace_fd_t* fd                                            );

        static std::int32_t create (char* path, std::uint8_t type                                 );
        static std::int32_t touch  (char* path                                                    );
    };

};