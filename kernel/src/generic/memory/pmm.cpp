
#include <generic/locks/spinlock.hpp>
#include <generic/memory/pmm.hpp>
#include <lib/limineA/limine.h>
#include <other/hhdm.hpp>
#include <drivers/serial/serial.hpp>
#include <other/string.hpp>
#include <config.hpp>
#include <other/log.hpp>

char pmm_spinlock = 0;
memory_entry_t biggest_entry;

void bitmapSetBit(uint64_t index) {
    if (index >= biggest_entry.bitmap.size) return;
    ((uint64_t*)(HHDM::toVirt(biggest_entry.bitmap.bits)))[index / 8] |= (1ULL << (index % 8));
}

void bitmapClearBit(uint64_t index) { 
    if (index >= biggest_entry.bitmap.size) return; 
    ((uint64_t*)(HHDM::toVirt(biggest_entry.bitmap.bits)))[index / 8] &= ~(1ULL << (index % 8));
}

int bitmapIsBitSet(uint64_t index) {
    if (index >= biggest_entry.bitmap.size) return 0; 
    return (((uint64_t*)(HHDM::toVirt(biggest_entry.bitmap.bits)))[index / 8] & (1ULL << (index % 8))) != 0;
}

void PMM::Init(limine_memmap_response* mem_map) {
    limine_memmap_entry* current_entry;
    uint64_t top;
    for(uint64_t i = 0; i < mem_map->entry_count;i++) {
        current_entry = mem_map->entries[i];
        Log("Entry #%d. Base: 0x%p, size: 0x%p, type: %s (%d)\n",i,current_entry->base,current_entry->length,current_entry->type == LIMINE_MEMMAP_USABLE ? "Usable" : "Non usable",current_entry->type);
        if(current_entry->type == LIMINE_MEMMAP_USABLE) {
            if(current_entry->length > top) {
                top = current_entry->length;
                biggest_entry.base = current_entry->base;
                biggest_entry.length = current_entry->length;
            }
        }
    }

    Serial::printf("Biggest entry: Base: 0x%p, Size: 0x%p (%d MB)\n",biggest_entry.base,biggest_entry.length,(biggest_entry.length / 1024) / 1024);

    uint64_t pageCount = biggest_entry.length / PAGE_SIZE;
    uint64_t bitmapSize = (pageCount + 7) / 8;
    uint64_t bitmapPages = bitmapSize / PAGE_SIZE;
    biggest_entry.bitmap.size = bitmapSize;
    biggest_entry.bitmap.bits = biggest_entry.base;
    biggest_entry.bitmap.page_count = pageCount;

    String::memset((void*)HHDM::toVirt(biggest_entry.base),0,bitmapSize);

    for(uint64_t i = 0;i < bitmapPages;i++) {
        bitmapSetBit(i);
    }
}

uint64_t PMM::Alloc() {
    spinlock_lock(&pmm_spinlock);
    uint64_t page = 0;
    for(uint64_t i = 0; i < biggest_entry.bitmap.page_count;i++) {
        if(!bitmapIsBitSet(i)) {
            bitmapSetBit(i);
            page = i;
            break;
        }
    }
    if(page)
        String::memset((void*)HHDM::toVirt(biggest_entry.base + (page * PAGE_SIZE)),0,PAGE_SIZE);
    spinlock_unlock(&pmm_spinlock);
    return page ? biggest_entry.base + (page * PAGE_SIZE) : 0;
}

void PMM::Free(uint64_t phys) {
    spinlock_lock(&pmm_spinlock);
    bitmapClearBit((phys - biggest_entry.base) / PAGE_SIZE);
    spinlock_unlock(&pmm_spinlock);
}

void* PMM::VirtualAlloc() {
    return (void*)HHDM::toVirt(Alloc());
}

void PMM::VirtualFree(void* ptr) {
    Free(HHDM::toPhys((uint64_t)ptr));
}

uint64_t PMM::BigAlloc(uint64_t size_pages) {
    spinlock_lock(&pmm_spinlock);
    uint64_t startDone = 0;
    uint64_t _continue = 0;
    for(uint64_t start = 0; start < biggest_entry.bitmap.page_count; start++) {
        if(!bitmapIsBitSet(start)) {
            _continue++;
            if(_continue == size_pages) {
                startDone = start - _continue + 1; 
                for(uint64_t i = startDone; i < startDone + size_pages; i++) {
                    bitmapSetBit(i);
                }
                break;
            }
        } else {
            _continue = 0; 
        }
    }
    spinlock_unlock(&pmm_spinlock);
    return startDone ? biggest_entry.base + (startDone * PAGE_SIZE) : 0;
}

void PMM::BigFree(uint64_t phys,uint64_t size_in_pages) {
    spinlock_lock(&pmm_spinlock);
    for(uint64_t i = (phys - biggest_entry.base) / PAGE_SIZE; i < ((phys - biggest_entry.base) / PAGE_SIZE) + size_in_pages;i++) {
        bitmapClearBit(i);
    }
    spinlock_unlock(&pmm_spinlock);
}

void* PMM::VirtualBigAlloc(uint64_t size_pages) {
    void* ptr = (void*)HHDM::toVirt(BigAlloc(size_pages));
    return ptr;
}

void PMM::VirtualBigFree(void* ptr,uint64_t size_in_pages) {
    BigFree(HHDM::toPhys((uint64_t)ptr),size_in_pages);
}