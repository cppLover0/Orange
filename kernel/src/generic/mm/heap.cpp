
#include <cstdint>
#include <cstddef>

#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>

#include <config.hpp>

std::uint8_t* heap_pool;
heap_block_t* heap_end;
heap_block_t* current;
void memory::heap::init() {
    heap_pool = (std::uint8_t*)memory::pmm::_virtual::alloc(KHEAP_SIZE);
    heap_block_t* block = (heap_block_t*)heap_pool;
    block->size = KHEAP_SIZE; 
    block->is_free = 1;
    block->next = 0;
    current = (heap_block_t*)heap_pool;
    heap_end = block;
}

void memory::heap::free(void* ptr) {
    if(ptr == 0)
        return;
    heap_block_t* block = (heap_block_t*)((std::uint64_t)ptr - sizeof(heap_block_t)); 
    block->is_free = 1;
    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
    }
}

void* memory::heap::malloc(std::uint32_t size) {
    size = (size + sizeof(heap_block_t) + 7) & ~7;

    if ((std::uint64_t)heap_end + size <= (std::uint64_t)heap_pool + KHEAP_SIZE) {
        heap_block_t* block = heap_end;
        block->size = size;
        block->is_free = 0;
        block->next = nullptr;

        heap_end = (heap_block_t*)((std::uint64_t)heap_end + size);

        return (void*)((std::uint64_t)block + sizeof(heap_block_t));
    }

    heap_block_t* start = current;
    do {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(heap_block_t)) {
                heap_block_t* new_block = (heap_block_t*)((char*)current + size);
                new_block->size = current->size - size;
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->is_free = 0;
            void* allocated = (void*)((char*)current + sizeof(heap_block_t));
            current = current->next ? current->next : (heap_block_t*)heap_pool;
            return allocated;
        }

        current = current->next ? current->next : (heap_block_t*)heap_pool;
    } while (current != start);

    return 0;
}
