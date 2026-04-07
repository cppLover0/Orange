#pragma once

#include <cstdint>
#include <generic/vfs.hpp>
#include <klibc/stdio.hpp>
#include <klibc/string.hpp>

namespace sysfs {
    void init(vfs::node* node);
    void create(const char* path, void* content, std::size_t size, ...);
    void create_symlink(const char* path, char* target_path, ...);
    void create_dir(const char* path, ...);
    void dump();
}