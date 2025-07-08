#include <etc/bootloaderinfo.hpp>
#include <generic/mm/pmm.hpp>
#include <etc/logging.hpp>
#include <etc/libc.hpp>
#include <etc/etc.hpp>
#include <limine.h>
#include <cstddef>
#include <cstdint>

buddy_t mem;

buddy_info_t* buddy_find_by_parent(std::uint64_t parent_id, char split_x) {
    for(std::uint64_t i = 0; i < mem.buddy_queue; i++) {
        if(mem.mem[i].parent_id == parent_id && mem.mem[i].split_x == split_x)
            return &mem.mem[i];
    }
    return nullptr;
}

buddy_info_t* buddy_find_by_phys(std::uint64_t phys) {
    for(std::uint64_t i = 0; i < mem.buddy_queue; i++) {
        if(mem.mem[i].phys == phys)
            return &mem.mem[i];
    }
    return nullptr;
}

buddy_info_t* memory::buddy::put(std::uint64_t phys, std::uint8_t level) {
    
    buddy_info_t* blud = &mem.mem[mem.buddy_queue++];
    blud->level = level;
    blud->phys = phys;
    blud->is_free = 1;
    blud->is_splitted = 0;
    blud->is_was_splitted = 0;
    blud->split_x = 0;
    blud->parent_id = 0;
    return blud;
}

buddy_info_t* memory::buddy::split_maximum(buddy_info_t* blud, std::uint64_t size) {
    if(!blud) return nullptr;
    
    std::uint64_t current_size = LEVEL_TO_SIZE(blud->level);
    std::uint64_t next_size = LEVEL_TO_SIZE(blud->level - 1);
    
    if((size <= current_size && next_size < size) || current_size == 4096)
        return blud;
        
    buddy_split_t split_pair = split(blud);
    if(!split_pair.first) return nullptr;
    
    return split_maximum(split_pair.first, size);
}

void memory::buddy::merge(uint64_t parent_id) {
    buddy_info_t* parent = &mem.mem[parent_id];
    if(!parent->is_was_splitted) return;
    
    buddy_info_t* bl = buddy_find_by_parent(parent_id, 0);
    buddy_info_t* ud = buddy_find_by_parent(parent_id, 1);
    
    if(!bl || !ud || !bl->is_free || !ud->is_free)
        return;

    bl->is_free = 0;
    ud->is_free = 0;
    parent->is_splitted = 0;
    parent->is_free = 1;
    
    if(parent->parent_id)
        merge(parent->parent_id);
}

buddy_split_t memory::buddy::split(buddy_info_t* info) {
    if(!info || LEVEL_TO_SIZE(info->level) == 4096 || !info->is_free)
        return {nullptr, nullptr};
        
    uint64_t daddy = ((uint64_t)info - (uint64_t)mem.mem) / sizeof(buddy_info_t);
    buddy_info_t* bl = nullptr;
    buddy_info_t* ud = nullptr;
    
    if(!info->is_was_splitted) {
        bl = put(info->phys, info->level - 1);
        ud = put(info->phys + LEVEL_TO_SIZE(info->level - 1), info->level - 1);
        if(!bl || !ud) return {nullptr, nullptr};
        
        bl->split_x = 0;
        bl->is_free = 1;
        bl->parent_id = daddy;
        ud->parent_id = daddy;
        ud->split_x = 1;
        ud->is_free = 1;
    } else {
        bl = buddy_find_by_parent(daddy, 0);
        ud = buddy_find_by_parent(daddy, 1);
        if(!bl || !ud) return {nullptr, nullptr};
        
        bl->is_free = 1;
        ud->is_free = 1;
    }
    
    info->is_splitted = 1;
    info->is_free = 0;
    info->is_was_splitted = 1;
    return {bl, ud};
}

int __buddy_align_power(uint64_t number) {
    if(number == 0) return 0;
    
    uint64_t power = 12;
    while(power < MAX_LEVEL && LEVEL_TO_SIZE(power) < number) {
        power++;
    }
    return power;
}

void memory::buddy::init() {
    limine_memmap_response* mmap = BootloaderInfo::AccessMemoryMap();
    if(!mmap || mmap->entry_count == 0) return;

    std::uint64_t top = 0, top_size = 0, total_pages = 0;
    for(int i = 0; i < mmap->entry_count; i++) {
        limine_memmap_entry* current = mmap->entries[i];
        if(current->type == LIMINE_MEMMAP_USABLE) {
            total_pages += current->length / 4096;
            if(current->length > top_size) {
                top = current->base;
                top_size = current->length;
            }
        }
    }

    std::uint64_t buddy_size = total_pages * sizeof(buddy_info_t);
    if(buddy_size > top_size) return;

    memset(&mem, 0, sizeof(buddy_t));
    mem.mem = (buddy_info_t*)Other::toVirt(top);
    memset(mem.mem, 0, buddy_size);

    std::uint64_t remaining_size = top_size - buddy_size;
    if(remaining_size < 4096) return;

    limine_memmap_entry* current = mmap->entries[0];
    for(int i = 0;i < mmap->entry_count; i++) {
        current = mmap->entries[i];
        if(current->type == LIMINE_MEMMAP_USABLE) {
            std::int64_t new_len,new_base = 0;
            new_len = current->length;
            new_base = current->base;
            if(new_base == top) {
                new_len = top_size - buddy_size;
                new_base = top + buddy_size;
            }
            while(new_len > 4096) {
                put(new_base,__buddy_align_power(new_len));
                new_base += LEVEL_TO_SIZE(__buddy_align_power(new_base));
                new_len -= LEVEL_TO_SIZE(__buddy_align_power(new_len));
            }
        }
    }
}

void memory::buddy::free(std::uint64_t phys) {
    auto blud = buddy_find_by_phys(phys);
    if(!blud || blud->is_splitted)
        return;
        
    blud->is_free = 1;
    if(blud->parent_id)
        merge(blud->parent_id);
}

std::int64_t memory::buddy::alloc(std::size_t size) {
    if(size == 0) return 0;
    if(size < 4096) size = 4096;

    buddy_info_t* best_fit = nullptr;
    std::uint64_t best_size = UINT64_MAX;

    for(std::uint64_t i = 0; i < mem.buddy_queue; i++) {
        if(mem.mem[i].is_free && !mem.mem[i].is_splitted) {
            std::uint64_t current_size = LEVEL_TO_SIZE(mem.mem[i].level);
            if(current_size >= size && current_size < best_size) {
                best_fit = &mem.mem[i];
                best_size = current_size;
            }
        }
    }

    if(!best_fit) return 0;

    auto blud = split_maximum(best_fit, size);
    if(!blud) return 0;

    blud->is_free = 0;
    memset(Other::toVirt(blud->phys), 0, LEVEL_TO_SIZE(blud->level));
    return blud->phys;
}

/* pmm wrapper */

void memory::pmm::_physical::init() {
    memory::buddy::init();
}

void memory::pmm::_physical::free(std::uint64_t phys) {
    memory::buddy::free(phys);
}

std::int64_t memory::pmm::_physical::alloc(std::size_t size) {
    return memory::buddy::alloc(size);
}

void memory::pmm::_virtual::free(void* virt) {
    memory::pmm::_physical::free(Other::toPhys(virt));
}

void* memory::pmm::_virtual::alloc(std::size_t size) {
    return Other::toVirt(memory::pmm::_physical::alloc(size));
}
