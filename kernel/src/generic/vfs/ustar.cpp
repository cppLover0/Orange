
#include <cstdint>

#include <generic/vfs/ustar.hpp>
#include <generic/vfs/vfs.hpp>

#include <etc/bootloaderinfo.hpp>
#include <etc/logging.hpp>
#include <limine.h>

#include <etc/etc.hpp>

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


void __ustar_fault(const char* msg) {

    Log::Display(LEVEL_MESSAGE_FAIL,"Can't continue kernel work ! initrd error, msg \"%s\"\n",msg);

    while(1) {
        asm volatile("hlt");
    }
}

extern std::uint8_t __tmpfs__create_parent_dirs_by_default;

void vfs::ustar::copy() {
    struct limine_module_response* initrd = BootloaderInfo::AccessInitrd();
    if(!initrd || initrd->module_count < 1)
        __ustar_fault("there's no initrd");

    ustar_header_t* current = (ustar_header_t*)initrd->modules[0]->address;
    std::uint64_t actual_tar_ptr_end = ((uint64_t)initrd->modules[0]->address + initrd->modules[0]->size) - 1024;
    while((std::uint64_t)current < actual_tar_ptr_end) {
        std::uint8_t type = oct2bin((uint8_t*)&current->type,1);
        switch(type) {
            case 0: {
                char* file = (char*)((uint64_t)current->file_name + 1);
                vfs::vfs::create(file,VFS_TYPE_FILE);
                int size = oct2bin((uint8_t*)current->file_size,strlen(current->file_size));
                
                userspace_fd_t fd;
                memset(&fd,0,sizeof(userspace_fd_t));
                memcpy(fd.path,file,strlen(file));
                
                vfs::vfs::write(&fd,(char*)((std::uint64_t)current + 512),size);
                vfs::vfs::var(&fd,(std::uint64_t)current->file_mode,TMPFS_VAR_CHMOD | (1 << 7));
                break;
            }

            case 5: {
                char* file = (char*)((uint64_t)current->file_name + 1);
                vfs::vfs::create(file,VFS_TYPE_DIRECTORY);
                break;
            }

            case 2: {
                char* file = (char*)((uint64_t)current->file_name + 1);
                userspace_fd_t fd;
                memset(&fd,0,sizeof(userspace_fd_t));
                memcpy(fd.path,file,strlen(file));
                vfs::vfs::create(file,VFS_TYPE_SYMLINK);
                vfs::vfs::write(&fd,current->name_linked,strlen(current->name_linked));
                break;
            }
        }
        current = (ustar_header_t*)((uint64_t)current + ALIGNUP(oct2bin((uint8_t*)&current->file_size,strlen(current->file_size)),512) + 512);
    }
    
    __tmpfs__create_parent_dirs_by_default = 0;

}