#pragma once

#include <cstdint>
#include <generic/lock/spinlock.hpp>

namespace ram_file {

    struct page {
        std::size_t num;
        void* p;
        page* next;
    };

    struct content {
        void* ident;
        std::int64_t inode;
        std::size_t ref_count;

        bool is_used;

        page* pages;
        content* next;
    };

    inline locks::spinlock _lock;

    inline void lock() { _lock.lock(); }
    inline void unlock() { _lock.unlock(); }

    content* create(std::int64_t inode, void* id);
    content* get(std::int64_t inode, void* id);

    page* access_page(std::int64_t inode, void* id, std::size_t page_num, bool should_create, void* desc, std::size_t original_mmap_off);
    void small(std::int64_t inode, void* id, std::size_t page_limit, void* lock_protection);

    void inc(std::int64_t inode, void* id);
    void dec(std::int64_t inode, void* id, void* lock_protection);
    
}