
#include <stdint.h>

typedef struct kernel_heap_block {
    uint64_t size;
    uint8_t isfree;
    struct kernel_heap_block* next;
} kernel_heap_block_t;

class KHeap {
public:
    static void* Malloc(uint64_t size);

    static void Free(void* ptr);

    static void Init();

};