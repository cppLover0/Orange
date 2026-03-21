#include <cstdint>
#include <generic/vfs.hpp>
#include <drivers/disk.hpp>
#include <klibc/string.hpp>
#include <utils/errno.hpp>
#include <klibc/stdio.hpp>
#include <generic/lock/mutex.hpp>

// /bin/path -> /usr/bin/path
void __vfs_symlink_resolve_no_at_symlink_follow(char* path, char* out) {
    char *next = nullptr;
    char buffer[4096];

    int e = vfs::readlink(path,buffer,4096);

    if(e == ENOSYS) 
        klibc::memcpy(out,path,klibc::strlen(path) + 1);

    if(e == EINVAL)
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    else if(e == 0) {
        
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    } else if(e == ENOENT) {
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
        // maybe it wants directory symlink ?
        char path0[4096];
        klibc::memcpy(path0,path,klibc::strlen(path) + 1);

        char result[4096];

        int c = 0;

        char* token = klibc::strtok(&next, path0, "/");
        
        /* Start building path from null and trying to find symlink */
        while(token) {
            
            result[c] = '/';
            c++;

            std::uint64_t mm = 0;
            mm = klibc::strlen(token);
            klibc::memcpy((char*)((std::uint64_t)result + c),token,mm);
            c += mm;
            result[c] = '\0';

            e = vfs::readlink(result,buffer,4096);
            if(e == 0) {
                char buffer2[4096];
                vfs::resolve_path(buffer,result,buffer2,0);
                c = klibc::strlen(buffer2);
                buffer2[c++] = '/';
                klibc::memcpy(buffer2 + c, path + klibc::strlen(result) + 1, klibc::strlen(path) - klibc::strlen(result));
                __vfs_symlink_resolve_no_at_symlink_follow(buffer2,out);
                return;
            } else if(e == ENOENT) {
                klibc::memcpy(out,path,klibc::strlen(path) + 1);
            }

            token = klibc::strtok(&next,0,"/");
        }
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    }
}

void __vfs_symlink_resolve(char* path, char* out, int level) {

    char *next = nullptr;

    char buffer[4096];

    if(level == 25) {
        // hell no :skull:
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
        return;
    }

    int e = vfs::readlink(path,buffer,4096);

    if(e == ENOSYS) 
        klibc::memcpy(out,path,klibc::strlen(path) + 1);

    if(e == EINVAL)
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    else if(e == 0) {
        
        char result[4096];
        vfs::resolve_path(buffer,path,result,0);
        __vfs_symlink_resolve(result,out, level + 1);
    } else if(e == ENOENT) {
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
        // maybe it wants directory symlink ?
        char path0[4096];
        klibc::memcpy(path0,path,klibc::strlen(path) + 1);

        char result[4096];

        int c = 0;

        char* token = klibc::strtok(&next, path0, "/");
        
        /* Start building path from null and trying to find symlink */
        while(token) {
            
            result[c] = '/';
            c++;

            std::uint64_t mm = 0;
            mm = klibc::strlen(token);
           klibc:: memcpy((char*)((std::uint64_t)result + c),token,mm);
            c += mm;
            result[c] = '\0';

            e = vfs::readlink(result,buffer,4096);
            if(e == 0) {
                char buffer2[4096];
                vfs::resolve_path(buffer,result,buffer2,0);
                c = klibc::strlen(buffer2);
                buffer2[c++] = '/';
                klibc::memcpy(buffer2 + c, path + klibc::strlen(result) + 1, klibc::strlen(path) - klibc::strlen(result));
                __vfs_symlink_resolve(buffer2,out, level + 1);
                return;
            } else if(e == ENOENT) {
                klibc::memcpy(out,path,klibc::strlen(path) + 1);
                return;
            } else {
                klibc::memcpy(out,path,klibc::strlen(path) + 1);
                return;
            }

            token = klibc::strtok(&next,0,"/");
        }
        klibc::memcpy(out,path,klibc::strlen(path) + 1);
    }
}


void vfs::init() {
    
}