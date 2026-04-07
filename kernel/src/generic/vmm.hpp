#pragma once

#include <generic/lock/spinlock.hpp>
#include <generic/hhdm.hpp>
#include <generic/paging.hpp>
#include <utils/assert.hpp>
#include <atomic>

#define MAP_FAILED ((void *)(-1))
#define MAP_FILE    0x00
#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANON      0x20
#define MAP_ANONYMOUS 0x20


struct vmm_obj {
    uint64_t base;
    uint64_t phys;
    uint64_t len;
    uint64_t flags;

    uint8_t is_free;
    uint8_t is_mapped;

    struct vmm_obj* next;
};

class vmm {
public:
    locks::preempt_spinlock lock;
    vmm_obj* start = nullptr;
    vmm_obj* end = nullptr;
    std::atomic<std::uint32_t> usage_counter = 0;

    std::uint64_t* root;

    vmm() {
        start = new vmm_obj;
        end = new vmm_obj;
        start->len = PAGE_SIZE;
        start->base = 0;
        start->next = end;
        end->base = etc::hhdm() - PAGE_SIZE;
        end->next = start;
        usage_counter = 1;
    }

    void init_root() {
        arch::copy_higher_half(*root, gobject::kernel_root);
    }

    vmm_obj* v_alloc(std::uint64_t len) {
        vmm_obj* current = this->start->next;
        vmm_obj* prev = start;

        uint64_t align_length = ALIGNUP(len,4096);
        len = align_length;

            
        while(current) {

            if(prev) {

                uint64_t prev_end = (prev->base + prev->len);
                if(current->base - prev_end >= align_length) {

                    vmm_obj* vmm_new = new vmm_obj;
                    vmm_new->base = prev_end;
                    vmm_new->len = align_length;
                    vmm_new->is_free = 0;
                    prev->next = vmm_new;
                    vmm_new->next = current;

                    vmm_new->is_mapped = 0;

                    return vmm_new;
                }
            }

            prev = current;
            current = current->next;
        }

        return 0;
    }

    vmm_obj* v_find(std::uint64_t base, std::uint64_t len) {
        vmm_obj* current = nullptr;
        vmm_obj* prev = nullptr;
        vmm_obj* vmm_new = new vmm_obj;

        current = this->start->next;
        prev = this->start;
            
        while(current) {
            if(prev) {
                uint64_t prev_end = (prev->base + prev->len);
                uint64_t current_end = base + len;

                if(current->base == base) {
                    assert(0, ":( vmm");
                    delete vmm_new;
                    vmm_new = current;
                    break;
                }

                    //DEBUG(1,"0x%p 0x%p 0x%p 0x%p",current->base,current_end,prev_end,base);

                if(current->base >= current_end && prev_end <= base) {
                    vmm_new->base = base;
                    vmm_new->len = len;
                    vmm_new->is_free = 0;
                    vmm_new->is_mapped = 0;
                    
                    prev->next = vmm_new;
                    vmm_new->next = current;
                    break;
                } 
                if(current == end)
                    assert(0, "help !");
            }

            prev = current;
            current = current->next;
        }

        return vmm_new;
    }

    void new_usage() {
        usage_counter++;
    }

    void free() {
        bool state = lock.lock();
        usage_counter--;
        if(usage_counter > 0) {lock.unlock(state); return;}

    }

    vmm_obj* nlgetlen(std::uint64_t addr) {
        vmm_obj* current = (vmm_obj*)this->start;

        while (current) {

            if (addr >= current->base && addr < current->base + current->len) {
                return current;
            }

            if (current == end)
                break;

            current = current->next;
        }
        return 0;
    }

    vmm_obj* getlen(std::uint64_t addr) {
        bool state = this->lock.lock();
        vmm_obj* current = (vmm_obj*)this->start;

        while (current) {

            if (addr >= current->base && addr < current->base + current->len) {
                this->lock.unlock(state);
                return current;
            }

            if (current == end)
                break;

            current = current->next;
        }
        this->lock.unlock(state);
        return 0;
    }

    void unmap(std::uint64_t base, std::uint64_t len, bool should_lock = true) {
        bool state = false;
        if(should_lock)
            state = this->lock.lock();
        vmm_obj* start = this->start;
        vmm_obj* current = start->next;
        vmm_obj* prev = start;

        vmm_obj* before = this->nlgetlen(base - 4096);
        vmm_obj* after = this->nlgetlen(base + len + 4096);
        len = ALIGNUP(len, 4096);

        if(before) {
            if(before->base == 0)
                before = 0;
        }

        if(after) {
            if(after == end) {
                after = 0;
            }
        }

        current = start->next;
        prev = start;

        if(before == after && before != 0 && after != 0) {

            if(before->is_mapped) {
                paging::zero_range(*this->root, before->base,before->len);
            }

            vmm_obj* split = new vmm_obj;
            klibc::memset(split,0,sizeof(vmm_obj));
            klibc::memcpy(split,before,sizeof(vmm_obj));
            split->len = (before->base + before->len) - (base + len);
            split->base = base + len;
            split->next = before->next;
            before->next = split;
            before->len = before->len - (len + split->len);
            current = split->next;
            goto end;
        } else {
            while(current) {
                if(prev) {
                    if(current == before && before != 0) {
                        if(before->base + before->len > base) {
                            if(before->is_mapped) {
                                paging::zero_range(*this->root, before->base,before->len);
                                
                            }
                            before->len -= ((before->base + before->len) - base);
                        }
                    } else {
                        
                        if(current->base >= base && current->base < (base + len)) {
                            if(current->is_mapped) {
                                paging::zero_range(*this->root, current->base,current->len);
                            }
                            std::uint64_t sz = ((base + len) -  current->base) > current->len ? current->len : current->len - ((current->base + current->len) - (base + len));
                            current->len -= sz;
                            current->base += sz;
                            if(current->len == 0) {
                                prev->next = current->next;
                                delete current;
                                current = prev->next;
                            }
                        }
                    }
                    if(current == end)
                        break;
                }
                prev = current;
                current = current->next;
            }
        }
end:
        paging::free_range(*this->root, base, len);
        paging::zero_range(*this->root, base, len);

        if(should_lock)
            this->lock.unlock(state);
            
    }

    std::uint64_t alloc_memory(std::uint64_t hint, std::uint64_t len, bool is_fixed) {
        vmm_obj* current = nullptr;
        bool state = this->lock.lock();
        if(is_fixed) {
            this->unmap(ALIGNDOWN(hint, PAGE_SIZE), ALIGNUP(len, PAGE_SIZE), false);
            current = this->v_find(ALIGNDOWN(hint, PAGE_SIZE), ALIGNUP(len, PAGE_SIZE));
        } else {
            current = this->v_alloc(ALIGNUP(len, PAGE_SIZE));
        }

        paging::zero_range(*this->root, current->base, current->len); // remove trash from page tables

        this->lock.unlock(state);
        return current->base;
    }

    std::uint64_t map_memory(std::uint64_t hint, std::uint64_t phys, std::uint64_t paging_flags, std::uint64_t len, bool is_fixed) {
        vmm_obj* current = nullptr;
        bool state = this->lock.lock();
        if(is_fixed) {
            this->unmap(ALIGNDOWN(hint, PAGE_SIZE), ALIGNUP(len, PAGE_SIZE), false);
            current = this->v_find(ALIGNDOWN(hint, PAGE_SIZE), ALIGNUP(len, PAGE_SIZE));
        } else {
            current = this->v_alloc(ALIGNUP(len, PAGE_SIZE));
        }

        current->is_mapped = true;
        current->phys = phys;
        current->flags = paging_flags;

        paging::zero_range(*this->root, current->base, current->len);
        paging::map_range(*this->root, current->phys, current->base, current->len, current->flags);

        this->lock.unlock(state);
        return current->base;
    }

    bool inv_lazy_alloc(std::uint64_t virt, std::uint64_t len) {
        bool is_allocated = false;
        // map_memory stuff should be already mapped so i can dont care about it 
        for(std::uint64_t i = 0;i < len; i += PAGE_SIZE) {
            std::int64_t current_phys = arch::get_phys_from_page(*this->root, virt + i);
            if(current_phys == 0 || current_phys == -1) {
                is_allocated = true;
                paging::map_range(*this->root, pmm::freelist::alloc_4k(), virt + i, PAGE_SIZE, PAGING_PRESENT | PAGING_RW | PAGING_USER);
            }
        }
        return is_allocated;
    }

};