
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

buddy_info_t* memory::buddy::put(std::uint64_t phys, std::uint8_t level) {
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

buddy_info_t* memory::buddy::split_maximum(buddy_info_t* blud, std::uint64_t size) {
    if((size <= LEVEL_TO_SIZE(blud->level) && LEVEL_TO_SIZE(blud->level - 1) < size) || LEVEL_TO_SIZE(blud->level) == 4096)
        return blud;
    return split_maximum(split(blud).first,size);
}

void memory::buddy::merge(buddy_info_t* budy) {
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
    blud->id = 0;
    bl->id = 0;
    ud->id = 0;
    if(blud->parent)
        merge(blud);
    return;
}

buddy_split_t memory::buddy::split(buddy_info_t* info) {
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

    mem.buddy_queue = 0;

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

int memory::buddy::free(std::uint64_t phys) {
    auto blud = buddy_find_by_phys_without_split(phys);
    if(!blud || blud->is_splitted)
        return -1;
    blud->is_free = 1;
    blud->id = 0;
    if(blud->parent)
        merge(blud);
    return 0;
}

int last_i = 0;

std::int64_t memory::buddy::alloc(std::size_t size) {
    std::uint64_t top_size = UINT64_MAX;
    buddy_info_t* nearest_buddy = 0;

    if(size < 4096)
        size = 4096;

    for(std::uint64_t i = 0;i < mem.buddy_queue; i++) {
        if(LEVEL_TO_SIZE(mem.mem[i].level) >= size && LEVEL_TO_SIZE(mem.mem[i].level) < top_size && mem.mem[i].is_free) {
            top_size = LEVEL_TO_SIZE(mem.mem[i].level);
            nearest_buddy = &mem.mem[i];
            if(top_size == size) {
                last_i = i; break;  }
        }
    }

found:
    if(nearest_buddy) {
        auto blud = split_maximum(nearest_buddy,size);
        blud->is_free = 0;
        memset(Other::toVirt(blud->phys),0,LEVEL_TO_SIZE(blud->level));
        return blud->phys;
    }

    assert(0,"There's no memory !");
    return 0;

}

alloc_t memory::buddy::alloc_ext(std::size_t size) {
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
        memset(Other::toVirt(blud->phys),0,LEVEL_TO_SIZE(blud->level));

        alloc_t result;
        result.real_size = LEVEL_TO_SIZE(blud->level);
        result.virt = blud->phys;

        return result;
    }

    assert(0,"There's no memory !");
    return {0,0};
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
            if(top_size == size)
                break;
        }
    }

    if(nearest_buddy) {
        auto blud = split_maximum(nearest_buddy,size);
        blud->is_free = 0;
        blud->id = id0;
        memset(Other::toVirt(blud->phys),0,LEVEL_TO_SIZE(blud->level));
        return blud->phys;
    }

    assert(0,"There's no memory !");
    return 0;
}

void memory::buddy::fullfree(std::uint32_t id) {
    for(std::uint64_t i = 0;i < mem.buddy_queue; i++) {
        if(mem.mem[i].id == id) {
            //free(mem.mem[i].phys);
        }
    }
}

/* freelist allocator */

template <int N>
struct freelist_allocated_memory {
    std::uint64_t arr[131072];
};

template <int N>
struct memory_id_block {
    std::uint64_t phys_block[N];
    struct memory_id_block* next;
};

typedef struct memory_id_block<4> memory_id_t;

typedef struct freelist_allocated_memory<1048576> freelist_allocated_memory_t;

std::uint64_t freelist_page = 0;

freelist_allocated_memory_t freelist_aloc_mem;
int freelist_aloc_mem_ptr = 0;

int memory::freelist::free(std::uint64_t phys) {
    if(phys == 0)
        return -1;
    // sorry but my freelist free is O(n) :(

    int success = 0;

    //Log::SerialDisplay(LEVEL_MESSAGE_INFO,"freelist: free 0x%p\n",phys);

    for(int i = 0; i < freelist_aloc_mem_ptr; i++) {
        if(phys >= freelist_aloc_mem.arr[i] && phys < freelist_aloc_mem.arr[i] + (1024 * 1024)) {
            success = 1;
            break;
        }
    }

    if(!success) {
        return -1; // not free list memory, ignore
    }

    memset(Other::toVirt(phys),0,4096);

    *((std::uint64_t*)Other::toVirt(phys)) = freelist_page;
    freelist_page = phys;
    return 0;
}

std::int64_t memory::freelist::alloc() {
    if(!freelist_page) { // request memory from buddy, 1 mb will be enough
        std::uint64_t phys = memory::buddy::alloc(1024 * 1024);

        freelist_aloc_mem.arr[freelist_aloc_mem_ptr++] = phys;

        std::uint64_t ptr = phys;
        while(1) {
            if(ptr >= phys + (1024 * 1024))
                break;
            free(ptr);
            ptr += 4096;
        }
    }
    std::uint64_t freelist_mem = freelist_page;

    freelist_page = *((std::uint64_t*)Other::toVirt(freelist_mem));

    memset(Other::toVirt(freelist_mem),0,4096);
    return freelist_mem;
}

/* pmm wrapper */

void memory::pmm::_physical::init() {
    memset(&freelist_aloc_mem,0,sizeof(freelist_aloc_mem));
    memory::buddy::init();
}

int __is_in_freelist_array(std::uint64_t phys) {
    for(int i = 0;i < freelist_aloc_mem_ptr; i++) {
        if(phys >= freelist_aloc_mem.arr[i] && phys < freelist_aloc_mem.arr[i] + (1024 * 1024))
            return 1;
    }
    return 0;
}

void memory::pmm::_physical::free(std::uint64_t phys) {
    return;
    asm volatile("cli");
    pmm_lock.lock();
    int status = -1;
    if(!__is_in_freelist_array(phys)) // not used by freelist ?
        status = memory::buddy::free(phys);
    if(status != 0) 
        memory::freelist::free(phys);
    pmm_lock.unlock();
}

void memory::pmm::_physical::fullfree(std::uint32_t id) {
    pmm_lock.lock();
    memory::buddy::fullfree(id);
    pmm_lock.unlock();
}

std::int64_t memory::pmm::_physical::alloc(std::size_t size) {
    asm volatile("cli");
    pmm_lock.lock();
    std::int64_t p = 0;
    if(size <= 4096) { // sure we can do freelist optimization
        p = memory::freelist::alloc();
    } else {
        p = memory::buddy::alloc(size);
    }
    pmm_lock.unlock();
    return p;
}

std::int64_t memory::pmm::_physical::allocid(std::size_t size, std::uint32_t id) {
    asm volatile("cli");
    pmm_lock.lock();
    std::int64_t p = 0;
    if(size <= 4096) { // sure we can do freelist optimization
        p = memory::freelist::alloc();
    } else {
        p = memory::buddy::allocid(size,id);
    }
    pmm_lock.unlock();
    return p;
}

void memory::pmm::_physical::lock() {
    pmm_lock.lock();
}

void memory::pmm::_physical::unlock() {
    pmm_lock.unlock();
}

void memory::pmm::_virtual::free(void* virt) {
    memory::pmm::_physical::free(Other::toPhys(virt));
}

void* memory::pmm::_virtual::alloc(std::size_t size) {
    asm volatile("cli");
    return Other::toVirt(memory::pmm::_physical::alloc(size));
}

alloc_t memory::pmm::_physical::alloc_ext(std::size_t size) {
    asm volatile("cli");
    pmm_lock.lock();
    alloc_t result;
    if(size <= 4096) {
        result.real_size = 4096;
        result.virt = memory::freelist::alloc();
    } else
        result = memory::buddy::alloc_ext(size);
    result.virt = (std::uint64_t)Other::toVirt(result.virt);
    pmm_lock.unlock();
    return result;
}

/* some helper functions */

#include <generic/mm/paging.hpp>

std::uint64_t memory::pmm::helper::alloc_kernel_stack(std::size_t size) {
    std::uint64_t stack = memory::pmm::_physical::alloc(size); /* extra page */
    memory::paging::alwaysmappedadd(stack,size);
    return (std::uint64_t)Other::toVirt(stack) + size;
}