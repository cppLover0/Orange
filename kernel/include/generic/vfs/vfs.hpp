
#include <cstdint>
#include <atomic>

#pragma once

#include <config.hpp>

#include <generic/mm/pmm.hpp>
#include <generic/locks/spinlock.hpp>
#include <etc/libc.hpp>

#include <etc/logging.hpp>

#define USERSPACE_FD_STATE_UNUSED 0
#define USERSPACE_FD_STATE_FILE 1
#define USERSPACE_FD_STATE_PIPE 2

#define VFS_TYPE_NONE 0
#define VFS_TYPE_FILE 1
#define VFS_TYPE_DIRECTORY 2
#define VFS_TYPE_SYMLINK 3

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 070
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010
#define S_IRWXO 07
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

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
        std::int64_t size = 0;
        std::uint64_t total_size = 0;
        std::uint64_t read_ptr = 0;
        locks::spinlock lock;
        std::atomic_flag is_received = ATOMIC_FLAG_INIT;
        std::atomic_flag is_n_closed = ATOMIC_FLAG_INIT;
        std::atomic_flag is_closed = ATOMIC_FLAG_INIT;
    public:

        std::uint64_t flags = 0;

        pipe(std::uint64_t flags) {
            this->buffer = (char*)memory::pmm::_virtual::alloc(USERSPACE_PIPE_SIZE);
            this->total_size = USERSPACE_PIPE_SIZE;
            this->size = 0;
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
                    this->is_closed.test_and_set();
                }
            }

            if(this->connected_to_pipe == 0) {
                delete this;
            }
            this->lock.unlock();
        }

        void create(std::uint8_t side) {
            this->lock.lock();
            this->connected_to_pipe++;
            if(side == PIPE_SIDE_WRITE)
                this->connected_to_pipe_write++;
            this->lock.unlock();
        }

        std::uint64_t force_write(const char* src_buffer, std::uint64_t count) {
            memcpy(this->buffer + this->size, src_buffer, count);
            this->size += count;
            return count;
        }

        std::uint64_t write(const char* src_buffer, std::uint64_t count) {
            std::uint64_t written = 0;
            while (written < count) {
                std::uint64_t space_left = total_size - size;
                if (space_left == 0) {
                    continue;
                }
                std::uint64_t to_write = (count - written < space_left) ? (count - written) : space_left;
                force_write(src_buffer + written, to_write);
                written += to_write;
                this->is_received.clear();
            }
            return written;
        }

        std::uint64_t read(char* dest_buffer, std::uint64_t count) {
            std::uint64_t read_bytes = 0;
            while (true) {
                if (this->size == 0 || this->read_ptr >= this->size) {
                    if (this->is_closed.test()) {
                        return 0;
                    }
                    if (flags & O_NONBLOCK) {
                        return 0;
                    }
                    continue;
                }
                std::int64_t size0 = this->size - this->read_ptr;
                read_bytes = (count < static_cast<std::uint64_t>(size0)) ? count : static_cast<std::uint64_t>(size0);
                memcpy(dest_buffer, this->buffer + this->read_ptr, read_bytes);
                this->read_ptr += read_bytes;
                if (this->read_ptr >= this->size) {
                    this->read_ptr = 0;
                    this->size = 0;
                    this->is_received.test_and_set();
                }
                break;
            }
            return read_bytes;
        }

        ~pipe() {
            memory::pmm::_virtual::free(this->buffer);
        }

    };

};

#define USERSPACE_FD_OTHERSTATE_MASTER 1
#define USERSPACE_FD_OTHERSTATE_SLAVE  2

/* It should be restored in dup2 syscall when oldfd is 0 */
typedef struct {
    std::int32_t index;
    std::uint8_t state;
    std::uint8_t pipe_side;
    vfs::pipe* pipe;
} userspace_old_fd_t;

typedef struct userspace_fd {
    std::uint64_t offset;
    std::int32_t index;
    std::uint8_t state;
    std::uint8_t pipe_side;

    std::uint8_t is_a_tty;

    std::uint8_t other_state;

    std::uint32_t queue;
    std::uint8_t cycle;

    vfs::pipe* pipe;
    char path[2048];

    struct userspace_fd* old_state;
    struct userspace_fd* next; /* Should be changed in fork() */

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

#define S_IFMT 0x0F000
#define S_IFBLK 0x06000
#define S_IFCHR 0x02000
#define S_IFIFO 0x01000
#define S_IFREG 0x08000
#define S_IFDIR 0x04000
#define S_IFLNK 0x0A000
#define S_IFSOCK 0x0C000

#define TMPFS_VAR_CHMOD 0
#define TMPFS_VAR_UNLINK 1
#define DEVFS_VAR_ISATTY 2

namespace vfs {

    static inline std::uint64_t resolve_count(char* str,std::uint64_t sptr,char delim) {
        char* current = str;
        std::uint16_t att = 0;
        std::uint64_t ptr = sptr;
        std::uint64_t ptr_count = 0;
        while(current[ptr] != delim) {
            if(att > 1024)
                return 0;
            att++;

            if(ptr != 0) {
                ptr_count++;
                ptr--;
            }
        }
        return ptr;
    }


    static inline int normalize_path(const char* src, char* dest, std::uint64_t dest_size) {
        if (!src ||!dest || dest_size < 2) return -1;
        std::uint64_t j = 0;
        int prev_slash = 0;
        for (std::uint64_t i = 0; src[i] && j < dest_size - 1; i++) {
            if (src[i] == '/') {
                if (!prev_slash) {
                    dest[j++] = '/';
                    prev_slash = 1;
                }
            } else {
                dest[j++] = src[i];
                prev_slash = 0;
            }
        }

        if (j > 1 && dest[j-1] == '/') j--;
        if (j >= dest_size) {
            dest[0] = '\0';
            return -1;
        }
        dest[j] = '\0';
        return 0;
    }

    /* I'll use path resolver from my old kernel */
    static inline void resolve_path(const char* inter0,const char* base, char *result, char spec, char is_follow_symlinks) {
        char buffer_in_stack[2048];
        char buffer2_in_stack[2048];
        char inter[2048];
        char* buffer = (char*)buffer_in_stack;
        char* final_buffer = (char*)buffer2_in_stack;
        std::uint64_t ptr = strlen((char*)base);
        char is_first = 1;
        char is_full = 0;

        memset(inter,0,2048);
        memset(buffer_in_stack,0,2048);
        memset(buffer2_in_stack,0,2048);

        memcpy(inter,inter0,strlen(inter0));
        memcpy(final_buffer,base,strlen((char*)base));

        if(strlen((char*)inter) == 1 && inter[0] == '.') {
            memset(result,0,2048);
            memcpy(result,base,strlen((char*)base));
            return;
        }

        if(!strcmp(inter,"/")) {
            memset(result,0,2048);
            memcpy(result,inter,strlen((char*)inter));
            return;
        }

        if(inter[0] == '/') {
            ptr = 0;
            memset(final_buffer,0,2048);
            is_full = 1;
        }

        if(spec)
            is_first = 0;

        buffer = strtok((char*)inter,"/");
        while(buffer) {

            if(is_first && !is_full) {
                std::uint64_t mm = resolve_count(final_buffer,ptr,'/');

                if(ptr < mm) {
                    final_buffer[0] = '/';
                    final_buffer[1] = '\0';
                    ptr = 1;
                    continue;
                } 

                ptr = mm;
                final_buffer[ptr] = '\0';
                is_first = 0;
            }

            if(!strcmp(buffer,"..")) {
                std::uint64_t mm = resolve_count(final_buffer,ptr,'/');

                if(!strcmp(final_buffer,"/\0")) {
                    buffer = strtok(0,"/");
                    continue;
                }
                    

                if(ptr < mm) {
                    final_buffer[0] = '/';
                    final_buffer[1] = '\0';
                    ptr = 1;
                    continue;
                } 

                ptr = mm;
                final_buffer[ptr] = '\0';
                


            } else if(strcmp(buffer,"./") && strcmp(buffer,".")) {

                final_buffer[ptr] = '/';
                ptr++;

                std::uint64_t mm = 0;
                mm = strlen(buffer);
                memcpy((char*)((std::uint64_t)final_buffer + ptr),buffer,mm);
                ptr += mm;
                final_buffer[ptr] = '\0';
            } 
            
            buffer = strtok(0,"/");
        }
        memset(result,0,2048);
        normalize_path(final_buffer,result,2048);
    }

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
        std::int32_t (*ioctl)  (userspace_fd_t* fd, char* path, unsigned long req, void *arg, int *res    );
        std::int32_t (*mmap)   (userspace_fd_t* fd, char* path, std::uint64_t* outp, std::uint64_t* outsz, std::uint64_t* outflags );
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
        static std::int64_t ioctl  (userspace_fd_t* fd, unsigned long req, void *arg, int *res    );
        static std::int32_t mmap   (userspace_fd_t* fd, std::uint64_t* outp, std::uint64_t* outsz, std::uint64_t* outflags);

        static std::int32_t create (char* path, std::uint8_t type                                 );
        static std::int32_t touch  (char* path                                                    );

        static std::int32_t nlstat (userspace_fd_t* fd, stat_t* out                               );

        static void unlock();
        static void lock();

    };

};