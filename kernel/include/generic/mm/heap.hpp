
#include <cstdint>
#include <cstddef>

#pragma once

typedef struct heap_block {
    std::uint32_t size;
    std::uint8_t is_free;
    struct heap_block* next;
} heap_block_t;

namespace memory {
    class heap {
        public:
            static void init();
            static void free(void* ptr);
            static void* malloc(std::uint32_t size);
    };
};