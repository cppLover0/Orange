#include <cstdint>
#include <generic/pmm.hpp>
#include <generic/bootloader/bootloader.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/hhdm.hpp>
#include <klibc/string.hpp>
#include <klibc/stdio.hpp>
#include <utils/align.hpp>
#include <atomic>

locks::spinlock pmm_lock;
std::uint64_t* freelist_hhdm = 0;
std::size_t memory_size = 0;

buddy_t mem;

buddy_info_t* buddy_find_by_parent(buddy_info_t* blud,char split_x) {
    if(split_x) {
        for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].parent == blud && mem.mem[i].split_x)
                return &mem.mem[i];
        }
    } else {
        for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].parent == blud && !mem.mem[i].split_x)
                return &mem.mem[i];
        }
    }
    return nullptr;
}

buddy_info_t* buddy_find_by_phys(std::uint64_t phys) {
    for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].phys == phys)
                return &mem.mem[i];
    }
    return 0;
}

buddy_info_t* buddy_find_by_phys_without_split(std::uint64_t phys) {
    for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].phys == phys && !mem.mem[i].is_splitted)
                return &mem.mem[i];
    }
    return 0;
}

buddy_info_t* pmm::buddy::put(std::uint64_t phys, std::uint8_t level) {
    buddy_info_t* blud = &mem.mem[mem.buddy_queue++];
    blud->level = level;
    blud->phys = phys;
    blud->is_free = 1;
    blud->is_splitted = 0;
    blud->is_was_splitted = 0;
    blud->parent = 0;
    blud->split_x = 0;
    return blud;
}

buddy_info_t* pmm::buddy::split_maximum(buddy_info_t* blud, std::uint64_t size) {
    if((size <= LEVEL_TO_SIZE(blud->level) && LEVEL_TO_SIZE(blud->level - 1) < size) || LEVEL_TO_SIZE(blud->level) == 4096)
        return blud;
    return split_maximum(split(blud).first,size);
}

void pmm::buddy::merge(buddy_info_t* budy) {
    buddy_info_t* bl = budy;
    buddy_info_t* ud = budy->twin;
    if(!bl || !ud || !bl->is_free || !ud->is_free)
        return;

    buddy_info_t* blud = budy->parent;
    bl->is_free = 0;
    ud->is_free = 0;
    blud->is_splitted = 0;
    blud->is_was_splitted = 1;
    blud->is_free = 1;
    if(blud->parent)
        merge(blud);
    return;
}

buddy_split_t pmm::buddy::split(buddy_info_t* info) {
    if(!info || LEVEL_TO_SIZE(info->level) == 4096 || !info->is_free)
        return {info,info};
    buddy_info_t* bl = 0;
    buddy_info_t* ud = 0;
    if(!info->is_was_splitted) {
        bl = put(info->phys,info->level - 1);
        ud = put(info->phys + LEVEL_TO_SIZE(info->level - 1), info->level - 1);
        bl->split_x = 0;
        bl->is_free = 1;
        ud->split_x = 1;
        ud->is_free = 1;
        bl->parent = info;
        ud->parent = info;
        bl->twin = ud;
        ud->twin = bl;
    } else {
        bl = buddy_find_by_parent(info,0);
        ud = bl->twin;
        bl->is_free = 1;
        ud->is_free = 1;
    }
    info->is_splitted = 1;
    info->is_free = 0;
    info->is_was_splitted = 1;
    return {bl,ud};
}

int __buddy_align_power(uint64_t number) {
    if (number == 0) return 0; 

    uint64_t power = 12; 

    while (LEVEL_TO_SIZE(power) <= number && power < MAX_LEVEL) {
        power++;
    }

    return power - 1; 
}

void pmm::buddy::init() {
    limine_memmap_response* mmap = bootloader::bootloader->get_memory_map();
    limine_memmap_entry* current = mmap->entries[0];
    std::uint64_t top,top_size,total_pages;
    top = 0;
    top_size = 0;
    total_pages = 0;
    total_pages = 0;
    for(std::size_t i = 0;i < mmap->entry_count; i++) {
        current = mmap->entries[i];
        if(current->type == LIMINE_MEMMAP_USABLE) {
            total_pages += current->length / 4096;
            memory_size += current->length;
            if(current->length > top_size) {
                top = current->base;
                top_size = current->length;
            }
        }
    }

    std::uint64_t buddy_size = (total_pages * sizeof(buddy_info_t));

    klibc::memset(&mem,0,sizeof(buddy_t));
    mem.mem = (buddy_info_t*)(top + etc::hhdm());
    klibc::memset(mem.mem,0,buddy_size);

    mem.buddy_queue = 0;

    for(std::size_t i = 0;i < mmap->entry_count; i++) {
        current = mmap->entries[i];
        if(current->type == LIMINE_MEMMAP_USABLE) {
            std::int64_t new_len = 0;
            std::uint64_t new_base = 0;
            new_len = current->length;
            new_base = current->base;
            if(new_base == top) {
                new_len = top_size - buddy_size;
                new_base = ALIGNUP(top + buddy_size,4096);
            }
            while(new_len > 4096) {
                put(new_base,__buddy_align_power(new_len));
                new_base += LEVEL_TO_SIZE(__buddy_align_power(new_len));
                new_len -= LEVEL_TO_SIZE(__buddy_align_power(new_len));
            }
        }
    }

}

int pmm::buddy::nlfree(std::uint64_t phys) {
    auto blud = buddy_find_by_phys_without_split(phys);
    if(!blud || blud->is_splitted)
        return -1;
    blud->is_free = 1;
    if(blud->parent)
        merge(blud);
    return 0;
}

alloc_t pmm::buddy::nlalloc_ext(std::size_t size) {
    std::uint64_t top_size = UINT64_MAX;
    buddy_info_t* nearest_buddy = 0;

    if(size < 4096)
        size = 4096;

    for(std::uint64_t i = 0;i < mem.buddy_queue; i++) {
        if(LEVEL_TO_SIZE(mem.mem[i].level) >= size && LEVEL_TO_SIZE(mem.mem[i].level) < top_size && mem.mem[i].is_free) {
            top_size = LEVEL_TO_SIZE(mem.mem[i].level);
            nearest_buddy = &mem.mem[i];
            if(top_size == size)
                break;
        }
    }

    if(nearest_buddy) {
        auto blud = split_maximum(nearest_buddy,size);
        blud->is_free = 0;
        klibc::memset((void*)(blud->phys + etc::hhdm()),0,LEVEL_TO_SIZE(blud->level));

        alloc_t result;
        result.real_size = LEVEL_TO_SIZE(blud->level);
        result.phys = blud->phys;

        return result;
    }

    klibc::printf("There's no memory !\n\r");
    return {0,0};
}

alloc_t pmm::buddy::alloc(std::size_t size) {
    pmm_lock.lock();
    alloc_t res = pmm::buddy::nlalloc_ext(size);
    pmm_lock.unlock();
    return res;
}

int pmm::buddy::free(std::uint64_t phys) {
    pmm_lock.lock();
    int res = pmm::buddy::nlfree(phys);
    pmm_lock.unlock();
    return res;
}

void pmm::init() {
    buddy::init();
    klibc::printf("PMM: Total usable memory: %lli bytes\r\n",memory_size);
}

std::uint64_t pmm::freelist::alloc_4k() {
    pmm_lock.lock();

    if(!freelist_hhdm) { // request memory from buddy, 4 mb will be enough
        alloc_t res = buddy::nlalloc_ext(1024 * 1024 * 4);
        std::uint64_t phys = res.phys;

        std::uint64_t ptr = phys;
        while(1) {
            if(ptr >= phys + (1024 * 1024 * 4))
                break;
            freelist::nlfree(ptr);
            ptr += PAGE_SIZE;
        }
    }

    std::uint64_t mem = (std::uint64_t)freelist_hhdm - etc::hhdm();
    freelist_hhdm = (std::uint64_t*)(*freelist_hhdm);
    klibc::memset((void*)(mem + etc::hhdm()),0,PAGE_SIZE);
    pmm_lock.unlock();
    return mem;
}

void pmm::freelist::nlfree(std::uint64_t phys) {
    std::uint64_t virt = phys + etc::hhdm();
    *(std::uint64_t**)virt = freelist_hhdm;
    freelist_hhdm = (std::uint64_t*)virt;
}

void pmm::freelist::free(std::uint64_t phys) {
    pmm_lock.lock();
    freelist::nlfree(phys);
    pmm_lock.unlock();
}