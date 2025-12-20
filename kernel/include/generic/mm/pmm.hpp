
#include <cstdint>
#include <cstddef>

#pragma once

#define LEVEL_TO_SIZE(level) (1U << (level))
#define MAX_LEVEL 32

typedef struct buddy_info {
    union {
        struct {
            std::uint16_t level : 8;
            std::uint16_t is_was_splitted : 1;
            std::uint16_t is_splitted : 1;
            std::uint16_t is_free : 1;
            std::uint16_t split_x : 1;
        };
        std::uint16_t buddy_info_raw;
    };
    struct buddy_info* parent;
    struct buddy_info* twin;
    std::uint32_t id;
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

typedef struct {
    std::uint64_t virt;
    std::uint64_t real_size; //optimization for tmpfs
} alloc_t;


namespace memory {

    class buddy {
    private:
        static buddy_info_t* split_maximum(buddy_info_t* blud, std::uint64_t size);
        static buddy_info_t* put(std::uint64_t phys, std::uint8_t level);
        static buddy_split_t split(buddy_info_t* info);
        static void merge(buddy_info_t* budy);
    public:
        static void init();
        static int free(std::uint64_t phys);
        static void fullfree(std::uint32_t id);
        static alloc_t alloc_ext(std::size_t size);
        static std::int64_t alloc(std::size_t size);
        static std::int64_t allocid(std::size_t size, std::uint32_t id);
    };

    class freelist {
    public:
        static int free(std::uint64_t phys);
        static std::int64_t alloc();
    };

    namespace pmm {

        
        class _physical {
        public:
            static void init();
            static void free(std::uint64_t phys);
            static void fullfree(std::uint32_t id);
            static alloc_t alloc_ext(std::size_t size);
            static std::int64_t alloc(std::size_t size);
            static std::int64_t allocid(std::size_t size, std::uint32_t id);

            static void lock();
            static void unlock();

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