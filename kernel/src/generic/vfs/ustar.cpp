
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

#define TAR_BLOCK_SIZE 512

std::uint64_t count_tar_files_simple(const unsigned char *data, size_t size) {
    int count = 0;
    size_t pos = 0;
    
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


extern const char* level_messages[];
void update_progress_bar(int current, int total, int bar_width) {
    static int last_percent = -1;
    
    int percent = 0;
    if (total > 0) {
        percent = (current * 100) / total;
    }
    
    if (percent == last_percent && current != total) {
        return;
    }
    last_percent = percent;
    
    int filled_width = 0;
    if (percent > 0) {
        filled_width = (percent * bar_width) / 100;
        if (filled_width > bar_width) {
            filled_width = bar_width;
        }
    }
    
    printf("\r%sLoading initrd ",level_messages[LEVEL_MESSAGE_INFO]);
    
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled_width) {
            printf("=");  
        } else if (i == filled_width && percent < 100) {
            printf(">");  
        } else {
            printf(" ");  
        }
    }
    printf("] %3d%% (%d/%d)", percent, current, total);
    
    if (current >= total) {
        printf("\n");
    }
    
}

extern std::uint8_t __tmpfs__create_parent_dirs_by_default;
extern std::uint8_t __tmpfs__dont_alloc_memory;


void vfs::ustar::copy() {
    struct limine_module_response* initrd = BootloaderInfo::AccessInitrd();
    if(!initrd || initrd->module_count < 1)
        __ustar_fault("there's no initrd");

    ustar_header_t* current = (ustar_header_t*)initrd->modules[0]->address;

    std::uint64_t total = count_tar_files_simple((std::uint8_t*)current,initrd->modules[0]->size);
    std::uint64_t currentp = 0;

    Log::Raw("%sLoading initrd ",level_messages[LEVEL_MESSAGE_INFO]);

    std::uint64_t actual_tar_ptr_end = ((uint64_t)initrd->modules[0]->address + initrd->modules[0]->size) - 1024;
    while((std::uint64_t)current < actual_tar_ptr_end) {
        std::uint8_t type = oct2bin((uint8_t*)&current->type,1);
        switch(type) {
            case 0: {
                char* file = (char*)((uint64_t)current->file_name + 1);
                int size = oct2bin((uint8_t*)current->file_size,strlen(current->file_size));
                std::uint64_t actual_mode = oct2bin((uint8_t*)current->file_mode,7);
                vfs::vfs::opt_create_and_write(file,VFS_TYPE_FILE,(char*)((std::uint64_t)current + 512),size,actual_mode);
                break;
            }

            case 5: {
                char* file = (char*)((uint64_t)current->file_name + 1);
                std::uint64_t actual_mode = oct2bin((uint8_t*)current->file_mode,7);
                vfs::vfs::opt_create_and_write(file,VFS_TYPE_DIRECTORY,0,0,actual_mode);
                break;
            }

            case 2: {
                char* file = (char*)((uint64_t)current->file_name + 1);
                std::uint64_t actual_mode = oct2bin((uint8_t*)current->file_mode,7);
                vfs::vfs::opt_create_and_write(file,VFS_TYPE_SYMLINK,current->name_linked,strlen(current->name_linked),actual_mode);
                break;
            }
        }
        currentp++;
        update_progress_bar(currentp,total,25);
        current = (ustar_header_t*)((uint64_t)current + ALIGNUP(oct2bin((uint8_t*)&current->file_size,strlen(current->file_size)),512) + 512);
    }
    
    __tmpfs__create_parent_dirs_by_default = 0;

}