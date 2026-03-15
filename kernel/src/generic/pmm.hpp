#pragma once

#include <cstdint>

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
    std::uint64_t phys;
    std::uint64_t real_size; //optimization for tmpfs
} alloc_t;


namespace pmm {
    void init();

    class buddy {
    private:
        static buddy_info_t* split_maximum(buddy_info_t* blud, std::uint64_t size);
        static buddy_info_t* put(std::uint64_t phys, std::uint8_t level);
        static buddy_split_t split(buddy_info_t* info);
        static void merge(buddy_info_t* budy);
    public:
        static void init();
        static int nlfree(std::uint64_t phys);
        static alloc_t nlalloc_ext(std::size_t size);

        static alloc_t alloc(std::size_t size);
        static int free(std::uint64_t phys);

    };

    namespace freelist {
        std::uint64_t alloc_4k();
        void free(std::uint64_t phys);
        void nlfree(std::uint64_t phys);
    };
};