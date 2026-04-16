#pragma once
#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/lock/mutex.hpp>
#include <utils/linux.hpp>
#include <utils/assert.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <generic/scheduling.hpp>
#include <generic/lock/spinlock.hpp>

struct statfs {
   long    f_type;  
   long    f_bsize;
   long    f_blocks;  
   long    f_bfree;   
   long    f_bavail;
   long    f_files; 
   long    f_ffree;    
   long    f_fsid;  
   long    f_namelen;  
   long    f_spare[6];
};

#define USERSPACE_PIPE_SIZE (64 * 1024)
#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000
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

// i asked clanker for writing some structs at least cuz im scared to have broken structs
struct statx {
    uint32_t stx_mask;         
    uint32_t stx_blksize;     
    uint64_t stx_attributes;  
    uint32_t stx_nlink;     
    uint32_t stx_uid;            
    uint32_t stx_gid;          
    uint16_t stx_mode;       
    uint16_t __spare0[1]; 
    uint64_t stx_ino;       
    uint64_t stx_size;   
    uint64_t stx_blocks;       
    uint64_t stx_attributes_mask; 

    struct { int64_t tv_sec; uint32_t tv_nsec; int32_t __reserved; } stx_atime;
    struct { int64_t tv_sec; uint32_t tv_nsec; int32_t __reserved; } stx_btime; 
    struct { int64_t tv_sec; uint32_t tv_nsec; int32_t __reserved; } stx_ctime;
    struct { int64_t tv_sec; uint32_t tv_nsec; int32_t __reserved; } stx_mtime;

    uint32_t stx_rdev_major;    
    uint32_t stx_rdev_minor;    
    uint32_t stx_dev_major;      
    uint32_t stx_dev_minor;    

    uint64_t stx_mnt_id;     
    uint64_t __spare2;        
    uint64_t __spare3[12];   
};


#define PIPE_SIDE_WRITE 1
#define PIPE_SIDE_READ 2

struct stat {
    unsigned long  st_dev;      /* ID устройства (8 байт) */
    unsigned long  st_ino;      /* Номер инoда (8 байт) */
    unsigned long  st_nlink;    /* Кол-во жестких ссылок (8 байт) */
    unsigned int   st_mode;     /* Тип файла и права (4 байта) */
    unsigned int   st_uid;      /* ID пользователя (4 байта) */
    unsigned int   st_gid;      /* ID группы (4 байта) */
    int            __pad0;      /* Выравнивание (4 байта) */
    unsigned long  st_rdev;     /* ID устройства спец. файла (8 байт) */
    long           st_size;     /* Размер в байтах (8 байт) */
    long           st_blksize;  /* Размер блока В/В (8 байт) */
    long           st_blocks;   /* Кол-во 512Б блоков (8 байт) */

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
    std::int32_t (*unlink)(filesystem* fs, char* path);

    char path[2048];
};

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
}

struct file_descriptor {
    
    file_descriptor_type type;

    std::size_t offset;
    std::uint32_t flags;
    std::int32_t index;

    union {
        std::uint64_t ino;
        std::uint64_t tmpfs_pointer;
        vfs::pipe* pipe;
    } fs_specific;

    struct {
        int cycle;
        int queue;
        int tty_num;
        bool is_a_tty;
        void* ls_pointer;
        bool is_master;
        int pipe_side;
        bool is_non_block; 
        bool is_cloexec;
    } other;

    struct {
        disk* target_disk;
        filesystem* fs;

        std::int32_t (*ioctl)(file_descriptor* file, std::uint64_t req, void* arg);
        signed long (*read)(file_descriptor* file, void* buffer, std::size_t count);
        signed long (*write)(file_descriptor* file, void* buffer, std::size_t count);
        std::int32_t (*stat)(file_descriptor* file, stat* out);
        void (*close)(file_descriptor* file);

        void (*ondup)();

        std::int32_t (*mmap)(file_descriptor* file, std::uint64_t* out_phys, std::size_t* out_size);

        signed long (*ls)(file_descriptor* file, char* out, std::size_t count);
        bool (*poll)(file_descriptor* file, vfs_poll_type type);

        std::int32_t (*chmod)(file_descriptor* file, int new_mode);
        std::int32_t (*zero)(file_descriptor* file);
    } vnode;

    file_descriptor* next;
    char path[3500];
};

#define CLOSE_RANGE_UNSHARE     (1U << 1)
#define CLOSE_RANGE_CLOEXEC     (1U << 2)

static_assert(sizeof(file_descriptor) < 4096, "no pls");

namespace vfs {

    class fdmanager {
    public:
        file_descriptor* head_fd = nullptr;
        locks::preempt_spinlock fd_lock;
        std::atomic<int> fd_usage_pointer = 0;

        file_descriptor* search(int idx) {
            bool state = this->fd_lock.lock();
            file_descriptor* current = this->head_fd;
            while(current) {
                if(current->index == idx && current->type != file_descriptor_type::unallocated) {
                    this->fd_lock.unlock(state);
                    return current;
                }
                current = current->next;
            }

            this->fd_lock.unlock(state);
            return nullptr;
        }

        file_descriptor* nl_search(int idx) {
            file_descriptor* current = this->head_fd;
            while(current) {
                if(current->index == idx) {
                    return current;
                }
                current = current->next;
            }

            return nullptr;
        }

        file_descriptor* createlowest(int idx) {
            bool state = this->fd_lock.lock();
            file_descriptor* current = this->head_fd;
            file_descriptor* lowest = nullptr;

            current = this->head_fd;
            while (current) {
                if (current->type == file_descriptor_type::unallocated && current->index > idx) {
                    if (!lowest || current->index < lowest->index) {
                        lowest = current;
                    }
                }
                current = current->next;
            }

            if (lowest == nullptr) {
                int candidate = idx + 1;
                bool found_collision;

                do {
                    found_collision = false;
                    current = this->head_fd;
                    while (current) {
                        if (current->index == candidate) {
                            candidate++; 
                            found_collision = true;
                            break;
                        }
                        current = current->next;
                    }
                } while (found_collision);

                lowest = (file_descriptor*)(pmm::freelist::alloc_4k() + etc::hhdm());
                lowest->index = candidate;
                lowest->next = head_fd;
                head_fd = lowest;
            }

            file_descriptor* next_ptr = lowest->next;
            int final_idx = lowest->index;

            klibc::memset(lowest, 0, sizeof(file_descriptor));
            lowest->next = next_ptr;
            lowest->index = final_idx;
            lowest->type = file_descriptor_type::file;

            this->fd_lock.unlock(state);
            return lowest;
        }


        void kill() {
            bool state = fd_lock.lock();
            fd_usage_pointer--;
            if(fd_usage_pointer > 0) {fd_lock.unlock(state); return; }
            file_descriptor* current = this->head_fd;
            while(current) {
                if(current->type != file_descriptor_type::unallocated) {
                    this->close(current); 
                }
                file_descriptor* next = current->next;
                pmm::freelist::free((std::uint64_t)current - etc::hhdm());
                current = next;
            } 
        }

        void new_usage() {
            fd_usage_pointer++;
        }

        void duplicate(fdmanager* dest) {
            bool state = fd_lock.lock();
            file_descriptor* current = this->head_fd;
            while(current) {
                file_descriptor* new_fd = (file_descriptor*)(pmm::freelist::alloc_4k() + etc::hhdm());
                klibc::memcpy(new_fd, current, sizeof(file_descriptor));
                new_fd->next = dest->head_fd;
                dest->head_fd = new_fd;
            
                if(new_fd->type == file_descriptor_type::pipe) {
                    new_fd->fs_specific.pipe->create(new_fd->other.pipe_side);
                } else if(new_fd->type == file_descriptor_type::file) {
                    if(new_fd->vnode.ondup)
                        new_fd->vnode.ondup();
                }

                current = current->next;
            }
            fd_lock.unlock(state);
        }

        void do_dup(file_descriptor* src, file_descriptor* new_fd) {
            bool state = fd_lock.lock();
            file_descriptor* next = new_fd->next;
            klibc::memcpy(new_fd, src, sizeof(file_descriptor));
            if(new_fd->type == file_descriptor_type::pipe) {
                new_fd->fs_specific.pipe->create(new_fd->other.pipe_side);
            } else if(new_fd->type == file_descriptor_type::file) {
                if(new_fd->vnode.ondup)
                    new_fd->vnode.ondup();
            }
            new_fd->next = next;
            fd_lock.unlock(state);
        } 

        void close_range(int first, int last, bool should_cloexec) {
            bool state = fd_lock.lock();
            file_descriptor* current = this->head_fd;
            while(current) {
                file_descriptor* next = current->next;
                if(current->index >= first && current->index <= last && current->type != file_descriptor_type::unallocated) {
                    if(should_cloexec) {
                        current->other.is_cloexec = true;
                    } else {
                        this->close(current);
                    }
                }
                current = next;
            }
            fd_lock.unlock(state);
        }

        file_descriptor* try_dup2(file_descriptor* src, int idx) {
            bool state = this->fd_lock.lock();
            file_descriptor* current = this->head_fd;
            file_descriptor* lowest = nullptr;

            current = this->head_fd;
            while (current) {
                if (current->index == idx) {
                    lowest = current;
                    break;
                }
                current = current->next;
            }

            if(lowest) {
                this->close(lowest);
            }

            if (lowest == nullptr) {
                lowest = (file_descriptor*)(pmm::freelist::alloc_4k() + etc::hhdm());
                lowest->index = idx;
                lowest->next = head_fd;
                head_fd = lowest;
            }

            file_descriptor* next_ptr = lowest->next;
            int final_idx = lowest->index;

            klibc::memcpy(lowest, src, sizeof(file_descriptor));
            lowest->next = next_ptr;
            lowest->index = final_idx;
            
            if(lowest->type == file_descriptor_type::pipe) {
                lowest->fs_specific.pipe->create(lowest->other.pipe_side);
            } else if(lowest->type == file_descriptor_type::file) {
                if(lowest->vnode.ondup)
                    lowest->vnode.ondup();
            }

            this->fd_lock.unlock(state);
            return lowest;
        }

        void close(file_descriptor* file) {
            if(file->type == file_descriptor_type::file) {
                if(file->vnode.close)
                    file->vnode.close(file);
            } else if(file->type == file_descriptor_type::pipe) {
                file->fs_specific.pipe->close(file->other.pipe_side);
            } else if(file->type != file_descriptor_type::unallocated) {
                assert(0, "unimplemented close type %d", file->type);
            }
            file->type = file_descriptor_type::unallocated;
        }

        void cloexec() {
            bool state = fd_lock.lock();
            file_descriptor* current = this->head_fd;
            while(current) {
                file_descriptor* next = current->next;
                if(current->other.is_cloexec)
                    this->close(current); 
                current = next;
            } 
            fd_lock.unlock(state);
        }

    };

    struct node {
        filesystem* fs;
        char internal_path[1024];
        char path[1024];
    };

    void init();
    std::int32_t open(file_descriptor* fd, char* path, bool follow_symlinks, bool is_directory);
    std::int32_t create(char* path, vfs_file_type type, std::uint32_t mode); 
    std::int32_t readlink(char* path, char* out, std::uint32_t out_len);
    std::int32_t rename(char* path, char* newpath);
    std::int32_t unlink(char* path);
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

// took from old kernel 
namespace vfs {

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
        char buffer2_in_stack[4096];
        char inter[4096];
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
        normalize_path(final_buffer,result,4096);

        if(result[0] == '\0') {
            result[0] = '/';
            result[1] = '\0';
        }
    }
}