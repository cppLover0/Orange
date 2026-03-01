#include <generic/heap.hpp>
#include <cstdint>

namespace klibc {
    
    void* malloc(std::size_t size) {
        return kheap::malloc(size);
    }

    void free(void* ptr) {
        kheap::free(ptr);
    }
    
};