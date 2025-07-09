
#include <cstdint>
#include <cstddef>

#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>

#include <config.hpp>

std::uint8_t* heap_pool;

void memory::heap::init() {
    heap_pool = (std::uint8_t*)memory::pmm::_virtual::alloc(KHEAP_SIZE);
    heap_block_t* block = (heap_block_t*)heap_pool;
    block->size = KHEAP_SIZE; 
    block->is_free = 1;
    block->next = 0;
}

void memory::heap::free(void* ptr) {
    heap_block_t* block = (heap_block_t*)((std::uint64_t)ptr - sizeof(heap_block_t)); 
    block->is_free = 1;
    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
    }
}

void* memory::heap::malloc(std::uint32_t size) {
    size = (size + sizeof(heap_block_t) + 7) & ~7;
    heap_block_t* current = (heap_block_t*)heap_pool;
    heap_block_t* prev = 0;
    
    while ((std::uint64_t)current < (std::uint64_t)heap_pool + KHEAP_SIZE) {
        if (current->is_free) {
            if (current->size >= size) {
                if (current->size > size + sizeof(heap_block_t)) {
                    heap_block_t* new_block = (heap_block_t*)((std::uint64_t)current + size);
                    new_block->size = current->size - size;
                    new_block->is_free = 1;
                    new_block->next = current->next;
                    
                    current->size = size;
                    current->next = new_block;
                }
                
                current->is_free = 0;
                return (void*)((std::uint64_t)current + sizeof(heap_block_t));
            }
        }
        
        prev = current;
        current = current->next;
        
        if (!current) {
            return 0;
        }
    }
    
    return 0;
}
