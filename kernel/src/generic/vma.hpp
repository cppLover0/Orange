#pragma once

// its not in vmm.hpp cuz vmm is designed for userspace

#include <cstdint>
#include <generic/lock/spinlock.hpp>
#include <atomic>

#include <generic/paging.hpp>
#include <generic/arch.hpp>
#include <generic/pmm.hpp>

#include <utils/rbtree.hpp>
#include <utils/align.hpp>
#include <utils/assert.hpp>

namespace vma {

    struct vma_object {
        std::uintptr_t base;
        std::uintptr_t len;
        bool is_mapped;
    };

    struct free_vma_object {
        std::uintptr_t base;
        std::uintptr_t size;
    };

    class _vma {
    private:
        utils::rbtree<vma_object, std::uintptr_t> tree;
        utils::rbtree<free_vma_object, std::uintptr_t> free_tree;

        std::atomic<std::uintptr_t> memory_ptr = 0; // should be used if there's no free memory in tree

        locks::spinlock lock;

        vma_object* create_memory(const std::size_t& length) {
            auto memory = this->free_tree.find_size(length);

            if(memory != nullptr) {

                auto copied_memory = *memory;

                vma_object new_mem = {};
                new_mem.base = memory->base;
                new_mem.len = length;
                this->free_tree.remove(new_mem.base);

                if(copied_memory.size > length) {
                    free_vma_object new_free_mem = {};
                    new_free_mem.base = copied_memory.base + length;
                    new_free_mem.size = copied_memory.size - length;
                    this->free_tree.insert_size(new_free_mem.base, new_free_mem.size, new_free_mem);
                }

                return this->tree.insert_object(new_mem.base, new_mem);
            }

            vma_object new_mem = {};
            new_mem.base = memory_ptr;
            new_mem.len = length;
            memory_ptr += length;

            return this->tree.insert_object(new_mem.base, new_mem);
        }

    public:
        _vma(const std::uintptr_t& start) {
            this->memory_ptr = start;
        }

        void* alloc(std::size_t len) {

            this->lock.lock();

            len = ALIGNPAGEUP(len);
            auto object = create_memory(len);

            for(std::uintptr_t address = object->base; address < object->base + object->len; address += PAGE_SIZE) {
                paging::map_range(gobject::kernel_root, pmm::freelist::alloc_4k(), address, PAGE_SIZE, PAGING_PRESENT | PAGING_RW);
            }

            arch::tlb_flush(object->base, object->len);

            this->lock.unlock();
            return (void*)object->base;
        }

        void* map(const std::uintptr_t& phys, std::size_t len, std::uintptr_t flags) {

            this->lock.lock();

            len = ALIGNPAGEUP(len);
            auto object = create_memory(len);

            paging::map_range(gobject::kernel_root, phys, object->base, len, PAGING_PRESENT | PAGING_RW | flags);

            arch::tlb_flush(object->base, object->len);
            object->is_mapped = true;

            this->lock.unlock();
            return (void*)object->base;
        }

        void free(void* base) {

            this->lock.lock();

            auto object = this->tree.get((std::uintptr_t)base);
            assert(object != nullptr, "big banana");

            if(object->is_mapped == false) {
                paging::free_range(gobject::kernel_root, object->base, object->len);
            }

            paging::zero_range(gobject::kernel_root, object->base, object->len);

            free_vma_object obj = {};
            obj.base = object->base;
            obj.size = object->len;

            this->free_tree.insert_size(obj.base, obj.size, obj);
            this->tree.remove((std::uintptr_t)base);

            this->lock.unlock();
        }
    };

    inline _vma* kmem = nullptr;
};