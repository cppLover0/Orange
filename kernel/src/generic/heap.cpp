// i used clanker to optimize some stuff that idk how to optimize/fix
#include <cstdint>
#include <generic/pmm.hpp>
#include <generic/heap.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/hhdm.hpp>
#include <klibc/stdio.hpp>
#include <klibc/string.hpp>

std::uint8_t* heap_pool;
locks::spinlock heap_lock;
std::uint8_t is_early;
std::uint64_t used_bytes = 0;
std::size_t pool_size;

heap_block* free_list = nullptr;

void kheap::init() {
    heap_pool = (std::uint8_t*)(pmm::buddy::alloc(KHEAP_SIZE).phys + etc::hhdm());
    pool_size = KHEAP_SIZE;
    heap_block* initial_block = (heap_block*)heap_pool;
    initial_block->size = KHEAP_SIZE - sizeof(heap_block);
    initial_block->is_free = true;
    initial_block->next = nullptr;
    free_list = initial_block;

    klibc::printf("KHeap: Initialized with %lli bytes\r\n", KHEAP_SIZE);
}

static void defragment() {
    if (!free_list) return;
    
    heap_block* curr = free_list;
    while (curr && curr->next) {
        if ((std::uint8_t*)curr + sizeof(heap_block) + curr->size == (std::uint8_t*)curr->next) {
            curr->size += sizeof(heap_block) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void* kheap::malloc(std::size_t size) {
    if (size == 0) return nullptr;

    size = (size + 7) & ~7;
    heap_lock.lock();

    defragment();

    heap_block* best = nullptr;
    heap_block* prev_best = nullptr;
    heap_block* curr = free_list;
    heap_block* prev = nullptr;
    std::size_t min_diff = ~0ULL;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            std::size_t diff = curr->size - size;
            if (diff < min_diff) {
                min_diff = diff;
                best = curr;
                prev_best = prev;
                if (diff == 0) break;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    if (best) {
        if (best->size >= size + sizeof(heap_block) + 8) {
            heap_block* next_block = (heap_block*)((std::uint8_t*)best + sizeof(heap_block) + size);
            next_block->size = best->size - size - sizeof(heap_block);
            next_block->is_free = true;
            next_block->next = best->next;
            
            if (prev_best) prev_best->next = next_block;
            else free_list = next_block;
            next_block->next = best->next;
            
            best->size = size;
            best->next = next_block;
        } else {
            if (prev_best) prev_best->next = best->next;
            else free_list = best->next;
        }
        
        best->is_free = false;
        used_bytes += best->size + sizeof(heap_block);
        heap_lock.unlock();
        klibc::memset((void*)((std::uint8_t*)best + sizeof(heap_block)), 0, size);
        return (void*)((std::uint8_t*)best + sizeof(heap_block));
    }

    heap_lock.unlock();
    return nullptr;
}

void kheap::free(void* ptr) {
    if (!ptr) return;

    heap_lock.lock();
    heap_block* block = (heap_block*)((std::uint8_t*)ptr - sizeof(heap_block));
    
    if (block->is_free) {
        heap_lock.unlock();
        return;
    }
    
    block->is_free = true;
    used_bytes -= (block->size + sizeof(heap_block));

    heap_block* curr = free_list;
    heap_block* prev = nullptr;
    
    while (curr && curr < block) {
        prev = curr;
        curr = curr->next;
    }
    
    block->next = curr;
    if (prev) {
        prev->next = block;
    } else {
        free_list = block;
    }
    
    heap_lock.unlock();
}

void* kheap::opt_malloc(std::size_t size) {
    return kheap::malloc(size);
}

void kheap::opt_free(void* ptr) {
    kheap::free(ptr);
}