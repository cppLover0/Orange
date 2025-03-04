
#include <stdint.h>
#include <generic/locks/spinlock.hpp>
#include <generic/memory/pmm.hpp>
#include <generic/memory/heap.hpp>
#include <generic/memory/paging.hpp>
#include <other/string.hpp>
#include <config.hpp>

char heap_lock = 0;
uint64_t kmalloc_base;

void KHeap::Init() {
    kernel_heap_block_t* head = (kernel_heap_block_t*)PMM::VirtualBigAlloc(SIZE_KHEAP_IN_PAGES);
    head->size = PAGE_SIZE * SIZE_KHEAP_IN_PAGES;
    head->isfree = 1;
    head->next = nullptr;
    kmalloc_base = (uint64_t)head;
    Paging::alwaysMappedAdd((uint64_t)head,PAGE_SIZE * SIZE_KHEAP_IN_PAGES);
}

kernel_heap_block_t* current = 0;

void* KHeap::Malloc(uint64_t size) {
    spinlock_lock(&heap_lock);
    if(!current) {
        current = (kernel_heap_block_t*)kmalloc_base;
    }
    uint64_t totalsize = size + sizeof(kernel_heap_block_t);
    while(current) {
        if(current->isfree && current->size >= totalsize) {
            if(current->size > totalsize + sizeof(kernel_heap_block_t)) {
                kernel_heap_block_t* nextblock = (kernel_heap_block_t*)((uint8_t*)current + totalsize);
                nextblock->size = current->size - totalsize;
                nextblock->isfree = 1;
                nextblock->next = current->next;
                current->size = totalsize;
                current->next = nextblock;
            }
            current->isfree = 0;
            spinlock_unlock(&heap_lock);
            return (void*)((uint8_t*)current + sizeof(kernel_heap_block_t));
        }
        current=current->next;
        if(!current) {
            current = (kernel_heap_block_t*)kmalloc_base;
        }
    }
    spinlock_unlock(&heap_lock);
    return (void*)0;
}

void KHeap::Free(void* ptr) {
    spinlock_lock(&heap_lock);
    kernel_heap_block_t* block = (kernel_heap_block_t*)((uint8_t*)ptr - sizeof(kernel_heap_block_t));
    block->isfree = 1;
    spinlock_unlock(&heap_lock);
}