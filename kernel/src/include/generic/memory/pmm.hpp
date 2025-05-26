
#include <stdint.h>
#include <lib/limineA/limine.h>

#pragma once

#define SIZE_TO_PAGES(size) (((size) + 4095) / 4096)
#define LEVEL_TO_SIZE(level) (1U << (level))

#define MAX_LEVEL 27 

typedef struct {
    int64_t level : 8;
    int64_t is_free : 1;
    int64_t is_splitted : 1;
    int64_t is_was_splitted : 1;
    int64_t split_x : 1;
    int64_t parent_id : 48;
} __attribute__((packed)) buddy_info_field_t;

typedef struct buddy_info {
    uint64_t phys_pointer;
    buddy_info_field_t information;
} __attribute__((packed)) buddy_info_t;

typedef struct {
    buddy_info_t* first_buddy;
    buddy_info_t* second_buddy;
} __attribute__((packed)) buddy_split_result_t;

typedef struct buddy_t {
    uint64_t hello_buddy;
    uint64_t total_available_size;
    buddy_info_t* mem;
} __attribute__((packed)) buddy_t;


class PMM {
public:
    static void Init(limine_memmap_response* mem_map);

    static uint64_t Alloc();

    static uint64_t BigAlloc(uint64_t size_pages);

    static void* VirtualAlloc();

    static void* VirtualBigAlloc(uint64_t size_pages);

    static void Free(uint64_t phys);
    
    static void VirtualFree(void* ptr);

};