
#include <stdint.h>
#include <lib/limine/limine.h>

typedef struct bitmap {
    uint64_t bits;
    uint64_t size;
    uint64_t page_count;
} bitmap_t;

typedef struct memory_entry {
    uint64_t base;
    uint64_t length;
    bitmap_t bitmap;
} memory_entry_t;

// it uses static methods
class PMM {
public:
    static void Init(limine_memmap_response* mem_map);

    static uint64_t Alloc();

    static uint64_t BigAlloc(uint64_t size_pages);

    static void* VirtualAlloc();

    static void* VirtualBigAlloc(uint64_t size_pages);

    static void Free(uint64_t phys);
    
    static void VirtualFree(void* ptr);

    static void BigFree(uint64_t phys,uint64_t size_in_pages);

    static void VirtualBigFree(void* ptr,uint64_t size_in_pages);

};