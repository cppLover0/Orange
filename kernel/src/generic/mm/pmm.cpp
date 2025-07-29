
#include <generic/locks/spinlock.hpp>
#include <etc/bootloaderinfo.hpp>
#include <generic/mm/pmm.hpp>
#include <etc/logging.hpp>
#include <etc/libc.hpp>
#include <etc/etc.hpp>
#include <limine.h>
#include <cstddef>
#include <cstdint>

/* buddy allocator */

locks::spinlock pmm_lock;
buddy_t mem;

buddy_info_t* buddy_find_by_parent(std::uint64_t parent_id,char split_x) {
    if(split_x) {
        for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].parent_id == parent_id && mem.mem[i].split_x)
                return &mem.mem[i];
        }
    } else {
        for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].parent_id == parent_id && !mem.mem[i].split_x)
                return &mem.mem[i];
        }
    }
}

buddy_info_t* buddy_find_by_phys(std::uint64_t phys) {
    for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].phys == phys)
                return &mem.mem[i];
    }
}

buddy_info_t* buddy_find_by_phys_without_split(std::uint64_t phys) {
    for(std::uint64_t i = 0;i < mem.buddy_queue;i++) {
            if(mem.mem[i].phys == phys && !mem.mem[i].is_splitted)
                return &mem.mem[i];
    }
}

buddy_info_t* memory::buddy::put(std::uint64_t phys, std::uint8_t level) {
    buddy_info_t* blud = &mem.mem[mem.buddy_queue++];
    blud->level = level;
    blud->phys = phys;
    blud->is_free = 1;
    blud->is_splitted = 0;
    blud->is_was_splitted = 0;
    blud->parent_id = 0;
    blud->split_x = 0;
    return blud;
}

buddy_info_t* memory::buddy::split_maximum(buddy_info_t* blud, std::uint64_t size) {
    if((size <= LEVEL_TO_SIZE(blud->level) && LEVEL_TO_SIZE(blud->level - 1) < size) || LEVEL_TO_SIZE(blud->level) == 4096)
        return blud;
    return split_maximum(split(blud).first,size);
}

void memory::buddy::merge(uint64_t parent_id) {
    buddy_info_t* bl = buddy_find_by_parent(parent_id,0);
    buddy_info_t* ud = buddy_find_by_parent(parent_id,1);
    if(!bl || !ud || !bl->is_free || !ud->is_free)
        return;

    buddy_info_t* blud = &mem.mem[parent_id];
    bl->is_free = 0;
    ud->is_free = 0;
    blud->is_splitted = 0;
    blud->is_was_splitted = 1;
    blud->is_free = 1;
    blud->id = 0;
    bl->id = 0;
    ud->id = 0;
    if(blud->parent_id)
        merge(blud->parent_id);
    return;
}

buddy_split_t memory::buddy::split(buddy_info_t* info) {
    if(!info || LEVEL_TO_SIZE(info->level) == 4096 || !info->is_free)
        return {info,info};
    uint64_t daddy = ((uint64_t)info - (uint64_t)mem.mem) / sizeof(buddy_info_t);
    buddy_info_t* bl = 0;
    buddy_info_t* ud = 0;
    if(!info->is_was_splitted) {
        bl = put(info->phys,info->level - 1);
        ud = put(info->phys + LEVEL_TO_SIZE(info->level - 1), info->level - 1);
        bl->split_x = 0;
        bl->is_free = 1;
        ud->split_x = 1;
        ud->is_free = 1;
        bl->parent_id = daddy;
        ud->parent_id = daddy;
    } else {
        bl = buddy_find_by_parent(daddy,0);
        ud = buddy_find_by_parent(daddy,1);
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

void memory::buddy::init() {
    limine_memmap_response* mmap = BootloaderInfo::AccessMemoryMap();
    limine_memmap_entry* current = mmap->entries[0];
    std::uint64_t top,top_size,total_pages;
    top = 0;
    top_size = 0;
    total_pages = 0;
    total_pages = 0;
    for(int i = 0;i < mmap->entry_count; i++) {
        current = mmap->entries[i];
        if(current->type == LIMINE_MEMMAP_USABLE) {
            total_pages += current->length / 4096;
            if(current->length > top_size) {
                top = current->base;
                top_size = current->length;
            }
        }
    }

    std::uint64_t buddy_size = (total_pages * sizeof(buddy_info_t));

    memset(&mem,0,sizeof(buddy_t));
    mem.mem = (buddy_info_t*)Other::toVirt(top);
    memset(mem.mem,0,buddy_size);

    for(int i = 0;i < mmap->entry_count; i++) {
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
                auto blud = put(new_base,__buddy_align_power(new_len));
                new_base += LEVEL_TO_SIZE(__buddy_align_power(new_len));
                new_len -= LEVEL_TO_SIZE(__buddy_align_power(new_len));
            }
        }
    }

}

void memory::buddy::free(std::uint64_t phys) {
    auto blud = buddy_find_by_phys_without_split(phys);
    if(!blud || blud->is_splitted)
        return;
    blud->is_free = 1;
    blud->id = 0;
    // if(blud->parent_id)
    //     merge(blud->parent_id);
}

std::int64_t memory::buddy::alloc(std::size_t size) {
    std::uint64_t top_size = UINT64_MAX;
    buddy_info_t* nearest_buddy = 0;

    if(size < 4096)
        size = 4096;

    for(std::uint64_t i = 0;i < mem.buddy_queue; i++) {
        if(LEVEL_TO_SIZE(mem.mem[i].level) >= size && LEVEL_TO_SIZE(mem.mem[i].level) < top_size && mem.mem[i].is_free) {
            top_size = LEVEL_TO_SIZE(mem.mem[i].level);
            nearest_buddy = &mem.mem[i];
        }
    }

    if(nearest_buddy) {
        auto blud = split_maximum(nearest_buddy,size);
        blud->is_free = 0;
        memset(Other::toVirt(blud->phys),0,LEVEL_TO_SIZE(blud->level));
        return blud->phys;
    }

    return 0;

}

std::int64_t memory::buddy::allocid(std::size_t size,std::uint32_t id0) {
    std::uint64_t top_size = UINT64_MAX;
    buddy_info_t* nearest_buddy = 0;

    if(size < 4096)
        size = 4096;

    for(std::uint64_t i = 0;i < mem.buddy_queue; i++) {
        if(LEVEL_TO_SIZE(mem.mem[i].level) >= size && LEVEL_TO_SIZE(mem.mem[i].level) < top_size && mem.mem[i].is_free) {
            top_size = LEVEL_TO_SIZE(mem.mem[i].level);
            nearest_buddy = &mem.mem[i];
        }
    }

    if(nearest_buddy) {
        auto blud = split_maximum(nearest_buddy,size);
        blud->is_free = 0;
        blud->id = id0;
        memset(Other::toVirt(blud->phys),0,LEVEL_TO_SIZE(blud->level));
        return blud->phys;
    }

    return 0;
}

void memory::buddy::fullfree(std::uint32_t id) {
    for(std::uint64_t i = 0;i < mem.buddy_queue; i++) {
        if(mem.mem[i].id == id) {
            free(mem.mem[i].phys);
        }
    }
}

/* pmm wrapper */

void memory::pmm::_physical::init() {
    memory::buddy::init();
}

void memory::pmm::_physical::free(std::uint64_t phys) {
    pmm_lock.lock();
    memory::buddy::free(phys);
    pmm_lock.unlock();
}

void memory::pmm::_physical::fullfree(std::uint32_t id) {
    memory::buddy::fullfree(id);
}

std::int64_t memory::pmm::_physical::alloc(std::size_t size) {
    pmm_lock.lock();
    std::int64_t p = memory::buddy::alloc(size);
    pmm_lock.unlock();
    return p;
}

std::int64_t memory::pmm::_physical::allocid(std::size_t size, std::uint32_t id) {
    pmm_lock.lock();
    std::int64_t p = memory::buddy::allocid(size,id);
    pmm_lock.unlock();
    return p;
}

void memory::pmm::_virtual::free(void* virt) {
    memory::pmm::_physical::free(Other::toPhys(virt));
}

void* memory::pmm::_virtual::alloc(std::size_t size) {
    return Other::toVirt(memory::pmm::_physical::alloc(size));
}

/* some helper functions */

#include <generic/mm/paging.hpp>

std::uint64_t memory::pmm::helper::alloc_kernel_stack(std::size_t size) {
    std::uint64_t stack = memory::pmm::_physical::alloc(size); /* extra page */
    memory::paging::alwaysmappedadd(stack,size);
    return (std::uint64_t)Other::toVirt(stack) + size;
}