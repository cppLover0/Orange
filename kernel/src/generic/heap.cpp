#include <cstdint>
#include <generic/pmm.hpp>
#include <generic/heap.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/hhdm.hpp>
#include <klibc/stdio.hpp>

std::uint8_t* heap_pool;
heap_block* heap_end;
heap_block* current;

locks::spinlock heap_lock;

void kheap::init() {
    heap_pool = (std::uint8_t*)(pmm::buddy::alloc(KHEAP_SIZE).phys + etc::hhdm());
    heap_block* block = (heap_block*)heap_pool;
    block->size = KHEAP_SIZE; 
    block->is_free = 1;
    block->next = 0;
    current = (heap_block*)heap_pool;
    heap_end = block;
    klibc::printf("KHeap: Available memory %lli bytes\r\n",KHEAP_SIZE);
    
}

int is_early = 1;

void kheap::opt_free(void* ptr) {
    if(!is_early) {
        kheap::free(ptr);
        return;
    }

    if(!ptr) return;
    heap_block* block = (heap_block*)((std::uint64_t)ptr - sizeof(heap_block)); 
    block->is_free = 1;
    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
    }
}

void* kheap::opt_malloc(std::size_t size) {

    if(!is_early) {
        return kheap::malloc(size);
    }

    size = (size + sizeof(heap_block) + 7) & ~7;

    if (1) {
        heap_block* block = heap_end;
        block->size = size;
        block->is_free = 0;
        block->next = nullptr;

        heap_end = (heap_block*)((std::uint64_t)heap_end + size);

        return (void*)((std::uint64_t)block + sizeof(heap_block));
    }
    return 0;
}

void kheap::free(void* ptr) {
    if(ptr == 0)
        return;

    heap_lock.lock();

    heap_block* block = (heap_block*)((std::uint64_t)ptr - sizeof(heap_block)); 
    block->is_free = 1;
    if (block->next && block->next->is_free) {
        block->size += block->next->size;
        block->next = block->next->next;
    }

    heap_lock.unlock();

}

void* kheap::malloc(std::size_t size) {

    heap_lock.lock();

    size = (size + sizeof(heap_block) + 7) & ~7;

    if ((std::uint64_t)heap_end + size <= (std::uint64_t)heap_pool + KHEAP_SIZE) {
        heap_block* block = heap_end;
        block->size = size;
        block->is_free = 0;
        block->next = nullptr;

        heap_end = (heap_block*)((std::uint64_t)heap_end + size);

        heap_lock.unlock();
        return (void*)((std::uint64_t)block + sizeof(heap_block));
    }

    heap_block* start = current;
    do {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(heap_block)) {
                heap_block* new_block = (heap_block*)((char*)current + size);
                new_block->size = current->size - size;
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->is_free = 0;
            void* allocated = (void*)((char*)current + sizeof(heap_block));
            current = current->next ? current->next : (heap_block*)heap_pool;
            heap_lock.unlock();
            return allocated;
        }

        current = current->next ? current->next : (heap_block*)heap_pool;
    } while (current != start);

    heap_lock.unlock();
    return 0;
}