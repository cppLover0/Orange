#include <generic/pmm.hpp>
#include <generic/shm.hpp>
#include <generic/scheduling.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/time.hpp>
#include <atomic>

static_assert(sizeof(shm_seg_t) < 4096, "shm_seg is bigger than page size !");

shm_seg_t* shm_head = nullptr;

std::atomic<int> shm_id_ptr = 1;

shm_seg_t* shm::shm_find_by_key(int key) {
    shm_seg_t* current = shm_head;
    assert(((std::uint64_t)shm_head > PAGE_SIZE) || shm_head == nullptr, "invalid shm 0x%p", shm_head);
    while(current) {
        assert(((std::uint64_t)current > PAGE_SIZE) || current == nullptr, "invalid shm (in while) 0x%p", current);
        if(current->key == (std::uint32_t)key)
            return current;
        current = current->next;
    }
    return 0;
}

shm_seg_t* shm::shm_find(int id) {
    shm_seg_t* current = shm_head;
    while(current) {
        if(current->id == (std::uint32_t)id)
            return current;
        current = current->next;
    }
    return 0;
}

void shm::shm_rm(shm_seg_t* seg) {
    shm_seg_t* prev = shm_head;
    while(prev) {
        if(prev->next == seg)
            break;
        prev = prev->next;
    }
    if(prev)
        prev->next = seg->next;
    else
        shm_head = nullptr;
    pmm::buddy::free(seg->phys);
    delete seg;
}

shm_seg_t* shm::shm_create(int key, std::size_t size) {
    shm_seg_t* new_seg = new shm_seg_t;
    klibc::memset(new_seg,0,sizeof(shm_seg_t));
    new_seg->next = shm_head;
    shm_head = new_seg;

    new_seg->key = key;
    new_seg->id = shm_id_ptr++;
    new_seg->len = size;
    new_seg->phys = pmm::buddy::alloc(new_seg->len).phys;
    return new_seg;
}