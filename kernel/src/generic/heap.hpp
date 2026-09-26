#pragma once
#include <cstdint>

#define KHEAP_SIZE (36 * 1024 * 1024)
#define HEAP_SIZE KHEAP_SIZE
struct heap_block {
    std::uint32_t size;
    std::uint16_t is_free;
    heap_block* next;
};

namespace kheap {

    enum class heap_flags {
        default_flag = 0,
        no_vma = 1
    };

    void* opt_malloc(std::size_t size);
    void opt_free(void* ptr);

    void init();
    void* malloc(std::size_t size, heap_flags flags = heap_flags::default_flag);
    void free(void* ptr);
}