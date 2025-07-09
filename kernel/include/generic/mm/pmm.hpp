
#include <cstdint>
#include <cstddef>

#pragma once

#define LEVEL_TO_SIZE(level) (1U << (level))
#define MAX_LEVEL 32

typedef struct buddy_info {
    union {
        struct {
            std::uint64_t level : 8;
            std::uint64_t is_was_splitted : 1;
            std::uint64_t is_splitted : 1;
            std::uint64_t is_free : 1;
            std::uint64_t split_x : 1;
            std::uint64_t parent_id : 48;
        };
        std::uint64_t buddy_info_raw;
    };
    std::uint64_t phys;
} __attribute__((packed)) buddy_info_t;

typedef struct {
    buddy_info_t* first;
    buddy_info_t* second;
} buddy_split_t;

typedef struct {
    uint64_t buddy_queue;
    uint64_t total_size;
    buddy_info_t* mem;
} __attribute__((packed)) buddy_t;

namespace memory {
    class buddy {
    private:
        static buddy_info_t* split_maximum(buddy_info_t* blud, std::uint64_t size);
        static buddy_info_t* put(std::uint64_t phys, std::uint8_t level);
        static buddy_split_t split(buddy_info_t* info);
        static void merge(uint64_t parent_id);
    public:
        static void init();
        static void free(std::uint64_t phys);
        static std::int64_t alloc(std::size_t size);
    };
    namespace pmm {
        class _physical {
        public:
            static void init();
            static void free(std::uint64_t phys);
            static std::int64_t alloc(std::size_t size);
        };
        class _virtual {
        public:
            static void free(void* virt);
            static void* alloc(std::size_t size);
        };
        class helper {
        public:
            static std::uint64_t alloc_kernel_stack(std::size_t size);
        };
    };
};