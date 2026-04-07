#include <cstdint>
#include <generic/vfs.hpp>
#include <generic/ustar.hpp>
#include <generic/tmpfs.hpp>
#include <klibc/stdio.hpp>
#include <utils/assert.hpp>
#include <utils/align.hpp>

inline int oct2bin(unsigned char *str, int size) {

    if(!str)
        return 0;

    std::uint64_t n = 0;
    volatile unsigned char *c = str;
    for(int i = 0;i < size;i++) {
        n *= 8;
        n += *c - '0';
        c++;
        
    }
    return n;
}


#define TAR_BLOCK_SIZE 512

std::uint64_t count_tar_files_simple(const unsigned char *data, std::size_t size) {
    int count = 0;
    std::size_t pos = 0;
    
    while (pos + TAR_BLOCK_SIZE <= size) {
        int all_zero = 1;
        for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
            if (data[pos + i] != 0) {
                all_zero = 0;
                break;
            }
        }
        
        if (all_zero) {
            if (pos + 2 * TAR_BLOCK_SIZE <= size) {
                all_zero = 1;
                for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
                    if (data[pos + TAR_BLOCK_SIZE + i] != 0) {
                        all_zero = 0;
                        break;
                    }
                }
                if (all_zero) break;
            }
        }
        
        count++;
        
        unsigned long file_size = 0;
        for (int i = 0; i < 11; i++) {
            unsigned char c = data[pos + 124 + i];
            if (c < '0' || c > '7') break;
            file_size = file_size * 8 + (c - '0');
        }
        
        unsigned long blocks = (file_size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
        pos += (1 + blocks) * TAR_BLOCK_SIZE;
        
        if (pos > size) break;
    }
    
    return count;
}

bool ustar::is_valid_ustar(char* archive, std::uint64_t len) {
    if(len < 1024)
        return false;

    if (klibc::memcmp(archive + 257, "ustar", 5) == 0) {
        return true;
    }
    return false;
}

extern std::uint8_t __tmpfs__create_parent_dirs_by_default;

void ustar::load(char* archive, std::uint64_t len) {

    ustar_header_t* current = (ustar_header_t*)archive;

    __tmpfs__create_parent_dirs_by_default = 1;

    std::uint64_t total = count_tar_files_simple((std::uint8_t*)current,len);

    std::uint64_t actual_tar_ptr_end = ((uint64_t)archive + len) - 1024;

    log("ustar", "loading initrd with total elements %lli", total);
    while((std::uint64_t)current < actual_tar_ptr_end) {
        std::uint8_t type = oct2bin((uint8_t*)&current->type,1);
        switch(type) {
            case 0: {
                char* file = (char*)((uint64_t)current->file_name);
                if(file[0] == '.')
                    file++;
                int size = oct2bin((uint8_t*)current->file_size,klibc::strlen(current->file_size));
                std::uint64_t actual_mode = oct2bin((uint8_t*)current->file_mode,7);
                file_descriptor fd = {};
                vfs::create(file, vfs_file_type::file, actual_mode);
                int status = vfs::open(&fd, file, false, false);
                assert(status == 0, "svfddfm %d", status);
                if(fd.vnode.write) {
                    fd.vnode.write(&fd, (char*)((std::uint64_t)current + 512),size);
                } else {
                    log("ustar", "there's no write support for file %s !", file);
                }

                char buffer[256];
                klibc::memset(buffer,0,256);
                fd.offset = 0;
                fd.vnode.read(&fd, buffer, 256);

                if(fd.vnode.close) {
                    fd.vnode.close(&fd);
                }
                break;
            }

            case 5: {
                char* file = (char*)((uint64_t)current->file_name);
                if(file[0] == '.')
                    file++;
                    
                if(file[0] != '/' && file[1] != '0') {
                    std::uint64_t actual_mode = oct2bin((uint8_t*)current->file_mode,7);
                    vfs::create(file, vfs_file_type::directory, actual_mode);
                }
                break;
            }

            case 2: {
                char* file = (char*)((uint64_t)current->file_name);
                if(file[0] == '.')
                    file++;
                std::uint64_t actual_mode = oct2bin((uint8_t*)current->file_mode,7);
                file_descriptor fd = {};
                vfs::create(file, vfs_file_type::symlink, actual_mode);
                int status = vfs::open(&fd, file, false, false);
                assert(status == 0, "svfddfm %d", status);
                if(fd.vnode.write) {
                    fd.vnode.write(&fd, current->name_linked,klibc::strlen(current->name_linked));
                } else {
                    log("ustar", "there's no write support for file %s !", file);
                }
                if(fd.vnode.close) {
                    fd.vnode.close(&fd);
                }
                break;
            }
        }
        
        current = (ustar_header_t*)((uint64_t)current + ALIGNUP(oct2bin((uint8_t*)&current->file_size,klibc::strlen(current->file_size)),512) + 512);
    }
    
    __tmpfs__create_parent_dirs_by_default = 0;
}
