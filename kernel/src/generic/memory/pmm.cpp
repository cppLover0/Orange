
#include <stdint.h>
#include <generic/limineA/limineinfo.hpp>
#include <generic/memory/pmm.hpp>
#include <other/log.hpp>
#include <other/hhdm.hpp>
#include <config.hpp>
#include <generic/memory/paging.hpp>
#include <generic/locks/spinlock.hpp>
#include <other/assembly.hpp>
#include <other/string.hpp>

buddy_t buddy;

char buddy_spinlock;

uint64_t __pmm_cache_offset = 0;
uint64_t test_free = 0;

buddy_info_t* buddy_put(uint64_t phys,int64_t parent,uint8_t level) {

    buddy_info_t* hello_buddy = &buddy.mem[buddy.hello_buddy++];
    hello_buddy->information.is_free = 1;
    hello_buddy->information.level = level;
    hello_buddy->information.parent_id = parent;

    hello_buddy->phys_pointer = phys;
    hello_buddy->information.is_splitted = 0;
    hello_buddy->information.is_was_splitted = 0;

    return hello_buddy;
}

buddy_info_t* buddy_find(uint64_t phys,char is_need_split) {
    for(int64_t i = 0; i < buddy.hello_buddy;i++)
        if(buddy.mem[i].phys_pointer == phys && buddy.mem[i].information.is_splitted == is_need_split)
            return &buddy.mem[i];
    return 0;
}

buddy_info_t* buddy_find_ign(uint64_t phys) {
    for(int64_t i = 0; i < buddy.hello_buddy;i++)
        if(buddy.mem[i].phys_pointer == phys)
            return &buddy.mem[i];
    return 0;
}

buddy_info_t* buddy_find_by_parent(uint64_t parent_id,uint8_t split_x) {
    if(split_x) {
        for(int64_t i = 0; i < buddy.hello_buddy;i++)
            if(buddy.mem[i].information.parent_id == parent_id && buddy.mem[i].information.split_x)
                return &buddy.mem[i];
    } else {
        for(int64_t i = 0; i < buddy.hello_buddy;i++)
            if(buddy.mem[i].information.parent_id == parent_id && !buddy.mem[i].information.split_x)
                return &buddy.mem[i];
    }
    return 0;
}

buddy_split_result_t buddy_split(uint64_t phys) {
    buddy_info_t* hi_buddy = buddy_find(phys,0);

    if(!hi_buddy)
        return {0,0};

    if(LEVEL_TO_SIZE(hi_buddy->information.level) == PAGE_SIZE) // i cant split this :(
        return {hi_buddy,hi_buddy};

    if(!hi_buddy->information.is_free)
        return {0,0};

    uint64_t parent_id = ((uint64_t)hi_buddy - (uint64_t)buddy.mem) / sizeof(buddy_info_t);

    buddy_info_t* hi = 0;
    buddy_info_t* _buddy = 0;
    
    if(!hi_buddy->information.is_was_splitted) {
        hi = buddy_put(hi_buddy->phys_pointer,parent_id,hi_buddy->information.level - 1);
        _buddy = buddy_put(hi_buddy->phys_pointer + LEVEL_TO_SIZE(hi_buddy->information.level - 1),parent_id,hi_buddy->information.level - 1);

        hi->information.split_x = 0;
        _buddy->information.split_x = 1;
        hi->information.is_free = 1;
        _buddy->information.is_free = 1;

    } else {
        
        hi = buddy_find_by_parent(parent_id,0);
        _buddy = buddy_find_by_parent(parent_id,1);

        hi->information.is_free = 1;
        _buddy->information.is_free = 1;

    }

    test_free += 2;

    hi_buddy->information.is_free = 0;
    hi_buddy->information.is_splitted = 1;
    hi_buddy->information.is_was_splitted = 1;


    return (buddy_split_result_t){hi,_buddy};

}

void buddy_merge(uint64_t parent_id) {
    buddy_info_t* hi = buddy_find_by_parent(parent_id,0);
    buddy_info_t* _buddy = buddy_find_by_parent(parent_id,1);

    if(!hi || !_buddy)
        return;

    if(!hi->information.is_free || !_buddy->information.is_free) // we can merge only if two are free
        return;

    buddy_info_t* hi_buddy = &buddy.mem[parent_id];

    hi->information.is_free = 0;
    _buddy->information.is_free = 0;
    hi_buddy->information.is_splitted = 0;
    hi_buddy->information.is_was_splitted = 1;
    hi_buddy->information.is_free = 1;

    test_free--;

    if(hi_buddy->information.parent_id) 
        buddy_merge(hi_buddy->information.parent_id); // merge again if possible

    return;

}

void buddy_free(uint64_t phys) {
    buddy_info_t* hi_buddy = buddy_find(phys,0);

    if(!hi_buddy)
        return;

    if(hi_buddy->information.is_splitted)
        return;

    hi_buddy->information.is_free = 1;
    test_free++;
    if(hi_buddy->information.parent_id) {
        buddy_merge(hi_buddy->information.parent_id);
    }

}

buddy_info_t* buddy_get_and_split_if_possible(buddy_info_t* buddy,uint64_t need_size) {

    if(buddy == 0) {
        Log(LOG_LEVEL_ERROR,"Allocator bug (buddy is 0)\n");
        return 0;
    }

    uint64_t buddy_size = LEVEL_TO_SIZE(buddy->information.level);
    uint64_t next_buddy_size = LEVEL_TO_SIZE(buddy->information.level - 1);

    uint64_t aligned_size = ALIGNPAGEDOWN(need_size);

    //Log("0x%p 0x%p 0x%p 0x%p\n",need_size,buddy_size,next_buddy_size);

    if((need_size <= buddy_size && next_buddy_size < need_size) || buddy_size == PAGE_SIZE)
        return buddy;


    if(buddy_size != PAGE_SIZE)
        return buddy_get_and_split_if_possible(buddy_split(buddy->phys_pointer).first_buddy,need_size);

}

uint64_t buddy_alloc(uint64_t size) {

    uint64_t top = UINT64_MAX;
    buddy_info_t* good_buddy = 0;

    if(size < 4096)
        size = 4096;

    // find a available buddy info which i need
    for(uint64_t i = 0;i < buddy.hello_buddy;i++) {
        if(LEVEL_TO_SIZE(buddy.mem[i].information.level) >= size && LEVEL_TO_SIZE(buddy.mem[i].information.level) < top && buddy.mem[i].information.is_free) {
            top = LEVEL_TO_SIZE(buddy.mem[i].information.level);
            good_buddy = &buddy.mem[i];
        }
    }

    if(good_buddy) { // we found a need buddy !!!
        buddy_info_t* good_buddy_split = buddy_get_and_split_if_possible(good_buddy,size);
        good_buddy_split->information.is_free = 0;

        String::memset((void*)HHDM::toVirt(good_buddy_split->phys_pointer),0,LEVEL_TO_SIZE(good_buddy_split->information.level));

        if(good_buddy_split->phys_pointer == 0) {
            ERROR("Buddy allocator bug\n");
        }

        test_free--;

        return good_buddy_split->phys_pointer;
    }

    ERROR("Camt find bud with size 0x%p:(\n",size);
    DEBUG("List all pages before hlt\n");
    int free = 0;
    int nonfree = 0;
    for(uint64_t  i = 0;i < buddy.hello_buddy;i++) {
        if(buddy.mem[i].information.is_free)
            NLog(" 0x%p:%d ",buddy.mem[i].phys_pointer,LEVEL_TO_SIZE(buddy.mem[i].information.level));
        if(buddy.mem[i].information.is_free)
            free++;
        else if(!buddy.mem[i].information.is_splitted)
            nonfree++;
    }

    NLog("\n");

    extern uint64_t paging_num_pages;

    DEBUG("Free: %d, NonFree: %d, absolute free: %d, paging_pages: %d\n",free,nonfree,test_free,paging_num_pages);

    __hlt();
    return 0;

}

void buddy_dump() {
    int free = 0;
    int nonfree = 0;
    uint64_t free_mem = 0;
    uint64_t total_mem = 0;
    for(uint64_t  i = 0;i < buddy.hello_buddy;i++) {

        if(buddy.mem[i].information.is_free)
            free_mem += LEVEL_TO_SIZE(buddy.mem[i].information.level);
        
        if(!buddy.mem[i].information.is_splitted)
            total_mem += LEVEL_TO_SIZE(buddy.mem[i].information.level);

        if(buddy.mem[i].information.is_free)
            free++;
        else if(!buddy.mem[i].information.is_splitted)
            nonfree++;
    }

    extern uint64_t paging_num_pages;

    NLog("Free: %d, NonFree: %d, paging_pages: %d\n",free,nonfree,paging_num_pages);
    NLog("Memory: %d/%d bytes, %d/%d KB, %d/%d MB\n",total_mem - free_mem,total_mem,(total_mem - free_mem) / 1024,total_mem / 1024,((total_mem - free_mem) / 1024) / 1024,(total_mem / 1024) / 1024);
}

int __buddy_align_power(uint64_t number) {
    if (number == 0) return 0; 

    uint64_t power = 12; 

    while (LEVEL_TO_SIZE(power) <= number && power < MAX_LEVEL) {
        power++;
    }

    return power - 1; 
}

void PMM::Init(limine_memmap_response* mem_map) {

    limine_memmap_entry* current = mem_map->entries[0]; 

    uint64_t top = 0;
    uint64_t top_size = 0;

    LimineInfo info;
    __pmm_cache_offset = info.hhdm_offset;

    uint64_t total_pages = 0;

    for(int i = 0;i < mem_map->entry_count;i++) {
        current = mem_map->entries[i];
        INFO("Entry %d: 0x%p-0x%p (%d MB) %s\n",i,current->base, current->base + current->length,(current->length / 1024) / 1024,current->type == LIMINE_MEMMAP_USABLE ? "Usable" : "Non-Usable");
    
        if(current->type == LIMINE_MEMMAP_USABLE) {
            total_pages += ALIGNPAGEDOWN(current->length) / PAGE_SIZE;
            if(current->length > top_size) {
                top = current->base;
                top_size = current->length;
            }
        }
    
    }

    INFO("Highest usable memory: 0x%p-0x%p (%d MB)\n",top,top + top_size,(top_size / 1024) / 1024);
    String::memset(&buddy,0,sizeof(buddy_t));

    uint64_t free_memory = top + (total_pages * sizeof(buddy_info_t));

    buddy.mem = (buddy_info_t*)HHDM::toVirt(top);
    String::memset(buddy.mem,0,(total_pages * sizeof(buddy_info_t)));

    INFO("Buddy allocator memory: 0x%p-0x%p ( size of buddy_info_t: %d, buddy_info_field_t: %d, buddy_t: %d, buddy_split_result_t: %d )\n",top,(uint64_t)top + ((top_size / PAGE_SIZE) * sizeof(buddy_info_t)),sizeof(buddy_info_t),sizeof(buddy_info_field_t),sizeof(buddy_t),sizeof(buddy_split_result_t));

    INFO("Putting all memory to buddy allocator\n");

    uint64_t final_size = top_size - (total_pages * sizeof(buddy_info_t));

    free_memory = ALIGNPAGEUP(free_memory);

    uint64_t max_level_align = LEVEL_TO_SIZE(MAX_LEVEL);
    for(int i = 0;i < mem_map->entry_count;i++) {
        current = mem_map->entries[i];
        if(current->type == LIMINE_MEMMAP_USABLE) {
            int64_t calc_len = current->length;
            uint64_t calc_base = current->base;
            if(current->base == top) {
                calc_base = free_memory;
                calc_len = final_size;
            }
            while(calc_len > 4096) {
                buddy_put(calc_base,0,__buddy_align_power(calc_len));
                calc_base += LEVEL_TO_SIZE(__buddy_align_power(calc_len));
                calc_len -= LEVEL_TO_SIZE(__buddy_align_power(calc_len));
                
            }
        }
    }

}

char pmm_spinlock = 0;

uint64_t PMM::Alloc() {
    spinlock_lock(&pmm_spinlock);
    uint64_t result = buddy_alloc(PAGE_SIZE);
    spinlock_unlock(&pmm_spinlock);
    return result;
}

uint64_t PMM::BigAlloc(uint64_t size_pages) {
    spinlock_lock(&pmm_spinlock);
    uint64_t result = buddy_alloc(PAGE_SIZE * size_pages);
    spinlock_unlock(&pmm_spinlock);
    return result;
}

void* PMM::VirtualAlloc() {
    return (void*)HHDM::toVirt(Alloc());
}

void* PMM::VirtualBigAlloc(uint64_t size_pages) {
    return (void*)HHDM::toVirt(BigAlloc(size_pages));
}

void PMM::Free(uint64_t phys) {
    spinlock_lock(&pmm_spinlock);
    buddy_free(phys);
    spinlock_unlock(&pmm_spinlock);
}
    
void PMM::VirtualFree(void* ptr) {
    Free(HHDM::toPhys((uint64_t)ptr));
}
