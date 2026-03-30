#pragma once
#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/lock/mutex.hpp>
#include <utils/linux.hpp>
#include <utils/assert.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <generic/scheduling.hpp>

#define USERSPACE_PIPE_SIZE (64 * 1024)
#define S_IFSOCK 0140000 
#define S_IFLNK  0120000 
#define S_IFREG  0100000  
#define S_IFBLK  0060000  
#define S_IFDIR  0040000  
#define S_IFCHR  0020000 
#define S_IFIFO  0010000  
#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

#define   CS8	0000060
#define ECHO	0000010   /* Enable echo.  */
#define IGNBRK	0000001  /* Ignore break condition.  */
#define BRKINT	0000002  /* Signal interrupt on break.  */
#define IGNPAR	0000004  /* Ignore characters with parity errors.  */
#define PARMRK	0000010  /* Mark parity and framing errors.  */
#define INPCK	0000020  /* Enable input parity check.  */
#define ISTRIP	0000040  /* Strip 8th bit off characters.  */
#define INLCR	0000100  /* Map NL to CR on input.  */
#define IGNCR	0000200  /* Ignore CR.  */
#define ICRNL	0000400  /* Map CR to NL on input.  */
#define OPOST	0000001  /* Post-process output.  */

#define ICANON	0000002 
#define VMIN 6

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

inline static std::uint32_t s_to_dt_type(std::uint32_t s_type) {
    switch (s_type)
    {
        case S_IFSOCK:
            return DT_SOCK;
        case S_IFREG:
            return DT_REG;
        case S_IFBLK:
            return DT_BLK;
        case S_IFIFO:
            return DT_FIFO;
        case S_IFDIR:
            return DT_DIR;
        case S_IFCHR:
            return DT_CHR;
        case S_IFLNK:
            return DT_LNK;
        default:
            assert(0,"shitfuck");
            return 0;
    }
    return 0;
}

enum class file_descriptor_type : std::uint8_t {
    unallocated = 0,
    file = 1,
    pipe = 2,
    socket = 3,
    epoll = 4,
    memfd = 5
};

struct dirent {
    std::uint64_t d_ino;   
    std::int64_t d_off;    
    unsigned short d_reclen; 
    unsigned char  d_type;  
    char d_name[];
};


struct stat {
    dev_t     st_dev;       
    ino_t     st_ino;         
    mode_t    st_mode;   
    nlink_t   st_nlink;       
    uid_t     st_uid;      
    gid_t     st_gid;        
    dev_t     st_rdev;      
    off_t     st_size;       
    blksize_t st_blksize;    
    blkcnt_t  st_blocks;     

    struct timespec st_atim;
    struct timespec st_mtim; 
    struct timespec st_ctim;  

#define st_atime st_atim.tv_sec    
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
};

enum class vfs_file_type : std::uint8_t {
    file = 1,
    directory = 2,
    symlink = 3
};

enum class vfs_poll_type : std::uint8_t {
    pollin = 1,
    pollout = 2
};

inline static std::uint32_t vfs_to_dt_type(vfs_file_type type) {
    switch(type) {
        case vfs_file_type::directory:
            return DT_DIR;
        case vfs_file_type::file:
            return DT_REG;
        case vfs_file_type::symlink:
            return DT_LNK;
        default:
            assert(0,"fsaifsi");
            return 0; //
    }
    return 0;
}

struct filesystem { 
    locks::mutex lock;

    union {
        std::uint64_t partition;
    } fs_specific;

    std::int32_t (*remove)(filesystem* fs, char* path);
    std::int32_t (*open)(filesystem* fs, void* file_desc, char* path, bool is_directory);
    std::int32_t (*readlink)(filesystem* fs, char* path, char* buffer);
    std::int32_t (*create)(filesystem* fs, char* path, vfs_file_type type, std::uint32_t mode);

    char path[2048];
};

struct file_descriptor {
    
    file_descriptor_type type;

    std::size_t offset;
    std::uint32_t flags;

    union {
        std::uint64_t ino;
        std::uint64_t tmpfs_pointer;
    } fs_specific;

    struct {
        int cycle;
        int queue;
        int tty_num;
        bool is_a_tty;
        void* ls_pointer;
    } other;

    struct {
        disk* target_disk;
        filesystem* fs;

        std::int32_t (*ioctl)(file_descriptor* file, std::uint64_t req, void* arg);
        signed long (*read)(file_descriptor* file, void* buffer, std::size_t count);
        signed long (*write)(file_descriptor* file, void* buffer, std::size_t count);
        std::int32_t (*stat)(file_descriptor* file, stat* out);
        void (*close)(file_descriptor* file);

        std::int32_t (*mmap)(file_descriptor* file, std::uint64_t* out_phys, std::size_t* out_size);

        signed long (*ls)(file_descriptor* file, char* out, std::size_t count);
        bool (*poll)(file_descriptor* file, vfs_poll_type type);

    } vnode;

};

namespace vfs {

    struct node {
        filesystem* fs;
        char path[2048];
    };

    void init();
    std::int32_t open(file_descriptor* fd, char* path, bool follow_symlinks, bool is_directory);
    std::int32_t create(char* path, vfs_file_type type, std::uint32_t mode); 
    std::int32_t readlink(char* path, char* out, std::uint32_t out_len);
    std::int32_t remove(char* path);
}

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS     19

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

typedef struct {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_line;
	cc_t c_cc[NCCS];
} __attribute__((packed)) termiosold_t;

#define PIPE_SIDE_WRITE 1
#define PIPE_SIDE_READ 2

// took from old kernel 
namespace vfs {

    class pipe {
    private:
        
        std::atomic_flag is_received = ATOMIC_FLAG_INIT;
        std::atomic_flag is_n_closed = ATOMIC_FLAG_INIT;
        

    public:

        char* buffer;

        locks::preempt_spinlock lock;

        std::atomic_flag is_closed = ATOMIC_FLAG_INIT;

        std::uint64_t total_size = 0;
        std::atomic<std::int64_t> size = 0;
        std::int64_t write_counter = 0;
        std::int64_t read_counter = 0;

        std::uint32_t connected_to_pipe = 0;
        std::uint32_t connected_to_pipe_write = 0;
        std::uint64_t flags = 0;

        std::uint32_t zero_message_count = 0;
        
        int is_closed_socket = 0;

        void* fd_pass = 0;
        pipe(std::uint64_t flags) {
            this->buffer = (char*)(pmm::buddy::alloc(USERSPACE_PIPE_SIZE).phys + etc::hhdm());
            this->total_size = USERSPACE_PIPE_SIZE;
            this->size = 0;
            this->connected_to_pipe = 2; /* syscall which creates pipe should create 2 fds too */ 
            this->connected_to_pipe_write = 1;
            this->flags = flags;

            this->is_closed.clear(std::memory_order_release);

        }

        void close(std::uint8_t side) {
            bool state = this->lock.lock();
            this->connected_to_pipe--;
            
            if(side == PIPE_SIDE_WRITE) {
                this->connected_to_pipe_write--;
                if(this->connected_to_pipe_write == 0) {
                    this->is_received.clear();
                    this->is_closed.test_and_set(std::memory_order_acquire);
                }
            }

            this->lock.unlock(state);

            if(this->connected_to_pipe == 0) {
                delete this;
            }
        }

        void create(std::uint8_t side) {
            bool state = this->lock.lock();
            this->connected_to_pipe++;
            if(side == PIPE_SIDE_WRITE)
                this->connected_to_pipe_write++;
            this->lock.unlock(state);
        }

        std::uint64_t force_write(const char* src_buffer, std::uint64_t count) {
            if (this->size + count > this->total_size) {
                count = this->total_size - this->size;
            }
            this->size += count;
            klibc::memcpy(this->buffer + (this->size - count), src_buffer, count);
            return count;
        }

        std::uint64_t write(const char* src_buffer, std::uint64_t count) {

            std::uint64_t written = 0;

            while (written < count) {
                bool state = this->lock.lock();

                std::uint64_t space_left = this->total_size - this->size;
                if (space_left == 0) {
                    this->lock.unlock(state);
                    process::yield();
                    continue;
                }

                std::uint64_t to_write = (count - written) < space_left ? (count - written) : space_left;
                if(to_write < 0)
                    to_write = 0;


                force_write(src_buffer + written, to_write);

                written += to_write;

                this->lock.unlock(state);
            }
            return written;
        }

        std::uint64_t nolock_write(const char* src_buffer, std::uint64_t count) {

            std::uint64_t written = 0;

            if(count == 0) {
                this->zero_message_count = 1;
                return 0;
            }

            while (written < count) {

                std::uint64_t space_left = this->total_size - this->size;

                std::uint64_t to_write = (count - written) < space_left ? (count - written) : space_left;
                if(to_write < 0)
                    to_write = 0;


                force_write(src_buffer + written, to_write);

                written += to_write;
                this->read_counter++;

            }
            return written;
        }

        std::int64_t read(char* dest_buffer, std::uint64_t count, int is_block) {

            std::uint64_t read_bytes = 0;

            while (true) {
                bool state = this->lock.lock();

                if (this->size == 0) {

                    if (this->is_closed.test(std::memory_order_acquire)) {
                        this->lock.unlock(state);
                        return 0; 
                    }

                    if(this->is_closed_socket) {
                        this->lock.unlock(state);
                        return 0;
                    }

                    if (flags & O_NONBLOCK || is_block) {
                        this->lock.unlock(state);
                        return -11;
                    }
                    this->lock.unlock(state);
                    process::yield();
                    continue;
                }
                
                read_bytes = (count < (std::uint64_t)this->size) ? count : (std::uint64_t)this->size;
                klibc::memcpy(dest_buffer, this->buffer, read_bytes);
                klibc::memmove(this->buffer, this->buffer + read_bytes, this->size - read_bytes);
                this->size -= read_bytes;

                this->lock.unlock(state);
                break;
            }
            return read_bytes;
        }



        ~pipe() {
            pmm::buddy::free((std::uint64_t)this->buffer - etc::hhdm());
        }

    };


    static inline std::uint64_t resolve_count(char* str,std::uint64_t sptr,char delim) {
        char* current = str;
        std::uint16_t att = 0;
        std::uint64_t ptr = sptr;
        while(current[ptr] != delim) {
            if(att > 1024)
                return 0;
            att++;

            if(ptr != 0) {
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
    static inline void resolve_path(const char* inter0,const char* base, char *result, char spec) {
        char* next = 0;
        char buffer2_in_stack[2048];
        char inter[2048];
        char* buffer = 0;
        char* final_buffer = (char*)buffer2_in_stack;
        std::uint64_t ptr = klibc::strlen((char*)base);
        char is_first = 1;
        char is_full = 0;

        klibc::memcpy(inter,inter0,klibc::strlen(inter0) + 1);

        if(klibc::strlen((char*)inter) == 1 && inter[0] == '.') {
            klibc::memcpy(result,base,klibc::strlen((char*)base) + 1);
            if(result[0] == '\0') {
                result[0] = '/';
                result[1] = '\0';
            }
            return;
        }

        if(!klibc::strcmp(inter,"/")) {
            klibc::memcpy(result,inter,klibc::strlen((char*)inter) + 1);
            if(result[0] == '\0') {
                result[0] = '/';
                result[1] = '\0';
            }
            return;
        } 

        if(inter[0] == '/') {
            ptr = 0;
            is_full = 1;
        } else {
            klibc::memcpy(final_buffer,base,klibc::strlen((char*)base) + 1);
        }

        if(spec)
            is_first = 0;

        if(!klibc::strcmp(base,"/"))                               
            is_first = 0;

        buffer = klibc::strtok(&next,(char*)inter,"/");
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

            if(!klibc::strcmp(buffer,"..")) {
                std::uint64_t mm = resolve_count(final_buffer,ptr,'/');

                if(!klibc::strcmp(final_buffer,"/\0")) {
                    buffer = klibc::strtok(&next,0,"/");
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
                


            } else if(klibc::strcmp(buffer,"./") && klibc::strcmp(buffer,".")) {

                final_buffer[ptr] = '/';
                ptr++;

                std::uint64_t mm = 0;
                mm = klibc::strlen(buffer);
                klibc::memcpy((char*)((std::uint64_t)final_buffer + ptr),buffer,mm);
                ptr += mm;
                final_buffer[ptr] = '\0';
            } 
            
            buffer = klibc::strtok(&next,0,"/");
        }
        normalize_path(final_buffer,result,2048);

        if(result[0] == '\0') {
            result[0] = '/';
            result[1] = '\0';
        }
    }
}