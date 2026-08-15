#include <cstdint>
#include <generic/procfs.hpp>
#include <klibc/stdio.hpp>
#include <klibc/string.hpp>
#include <klibc/stdlib.hpp>
#include <generic/userspace/syscall_list.hpp>
#include <utils/errno.hpp>

int count_slash(const char* str) {
    int idx = 0;
    int c = 0;
    while(str[idx] != '\0') {
        if(str[idx] == '/')
            c++;
        idx++;
    }
    return c;
}

std::int32_t procfs_readlink(filesystem* fs, char* path, char* buffer) {
    (void)fs;
    int slash_count = count_slash(path);
    klibc::memset(buffer, 0, 4096);
    if(slash_count == 1) {
        if(klibc::strcmp(path, "/self\n") == 0) {
            // points to current pid
            klibc::__printfbuf(buffer, 4096, "/%d", current_proc->pid);
            return 0;
        } else {
            return -EINVAL;
        }
    }
    return -EINVAL;
}


void procfs::init(vfs::node* node) {
    filesystem* new_fs = new filesystem;
    node->fs = new_fs;
    node->fs->readlink = procfs_readlink; 
}