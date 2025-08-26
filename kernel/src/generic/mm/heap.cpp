
#include <cstdint>
#include <cstddef>

#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>
#include <generic/mm/paging.hpp>

#include <etc/logging.hpp>

#include <config.hpp>

#include <generic/locks/spinlock.hpp>

#include <etc/etc.hpp>

std::uint8_t* heap_pool;
heap_block_t* heap_end;
heap_block_t* current;

locks::spinlock heap_lock;

void memory::heap::init() {
    heap_pool = (std::uint8_t*)memory::pmm::_virtual::alloc(KHEAP_SIZE);
    heap_block_t* block = (heap_block_t*)heap_pool;
    block->size = KHEAP_SIZE; 
    block->is_free = 1;
    block->next = 0;
    current = (heap_block_t*)heap_pool;
    heap_end = block;
    
    Log::Display(LEVEL_MESSAGE_OK,"heap_pool: 0x%p\n",heap_pool);
    memory::paging::alwaysmappedadd(Other::toPhys(heap_pool),KHEAP_SIZE);
}

void memory::heap::lock() {
    heap_lock.lock();
}

void memory::heap::unlock() {
    heap_lock.unlock();
}

void memory::heap::free(void* ptr) {
    if(ptr == 0)
        return;

    heap_lock.lock();

    heap_block_t* block = (heap_block_t*)((std::uint64_t)ptr - sizeof(heap_block_t)); 
    block->is_free = 1;
    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
    }

    heap_lock.unlock();

}

void* memory::heap::malloc(std::uint32_t size) {

    heap_lock.lock();

    size = (size + sizeof(heap_block_t) + 7) & ~7;

    if ((std::uint64_t)heap_end + size <= (std::uint64_t)heap_pool + KHEAP_SIZE) {
        heap_block_t* block = heap_end;
        block->size = size;
        block->is_free = 0;
        block->next = nullptr;

        heap_end = (heap_block_t*)((std::uint64_t)heap_end + size);

        heap_lock.unlock();
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
            heap_lock.unlock();
            return allocated;
        }

        current = current->next ? current->next : (heap_block_t*)heap_pool;
    } while (current != start);

    heap_lock.unlock();
    return 0;
}
