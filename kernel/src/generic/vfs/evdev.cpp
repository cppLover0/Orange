
#include <generic/vfs/vfs.hpp>
#include <generic/vfs/devfs.hpp>
#include <generic/vfs/evdev.hpp>

#include <cstdint>

vfs::evdev_node* evdev_head_node = 0;

int evdev_num = 0;

inline bool isdigit(char ch) {
    return ch >= '0' && ch <= '9';
}

inline long reverse_number(long num) {
    long reversed_num = 0;
    
    while(num > 0) {
        int digit = num % 10;     
        reversed_num = reversed_num * 10 + digit; 
        num /= 10;                
    }
    
    return reversed_num;
}


vfs::evdev_node* evdev_find_dev(const char* loc) {
    vfs::evdev_node* dev = evdev_head_node;
    char temp[256]; 
    int len = strlen((char*)loc);
    int i = len - 1;

    evdev_num = 0;

    while (i >= 0 && isdigit(loc[i])) {
        evdev_num = evdev_num * 10 + (loc[i] - '0'); 
        i--;
    }

    evdev_num = reverse_number(evdev_num);

    memcpy(temp, loc, i + 1);
    temp[i + 1] = '\0'; 

    while (dev) {
        if (!strcmp(temp,"/event") && evdev_num == dev->num) {
            return dev;
        } 
        dev = dev->next;
    }

    return 0;
}

std::int32_t __evdev__open(userspace_fd_t* fd, char* path) {
    vfs::evdev_node* node = evdev_find_dev(path);
    if(!node)
        return EINVAL;
    return 0;
}

void vfs::evdev::mount(vfs_node_t* node) {
    evdev_head_node = (evdev_node*)memory::pmm::_virtual::alloc(4096);
    node->open = __evdev__open;
}