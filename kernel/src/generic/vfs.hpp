#pragma once
#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/lock/mutex.hpp>
#include <utils/linux.hpp>

enum class file_descriptor_type : std::uint8_t {
    unallocated = 0,
    file = 1,
    pipe = 2,
    socket = 3,
    epoll = 4,
    memfd = 5
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

struct filesystem { 
    locks::mutex lock;

    union {
        std::uint64_t partition;
    } fs_specific;

    std::uint32_t (*open)(filesystem* fs, void* file_desc, char* path);
    
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
        disk* target_disk;
        filesystem* fs;

        signed long (*read)(file_descriptor* file, void* buffer, signed long count);
        signed long (*write)(file_descriptor* file, void* buffer, signed long count);
        std::int32_t (*stat)(file_descriptor* file, stat* out);
        void (*close)(file_descriptor* file);
    } vnode;

};

namespace vfs {

    void init();
    std::int32_t open(file_descriptor* fd, char* path, bool follow_symlinks);
    std::int32_t create(char* path, std::uint32_t mode); // mode contains type too
    std::int32_t readlink(char* path, char* out, std::uint32_t out_len);
}

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