#pragma once

// its not in vmm.hpp cuz vmm is designed for userspace

#include <cstdint>
#include <generic/lock/spinlock.hpp>
#include <atomic>

#include <utils/align.hpp>

namespace vma {

    struct vma_object {
        std::uintptr_t base;
        std::uintptr_t len;
    };

    class _vma {
    private:
        //vma_object* head = nullptr;
    public:
        _vma(std::uintptr_t start, std::uintptr_t end) {
            (void)end;
            (void)start;
        }

        void* alloc(std::size_t len) {
            len = ALIGNPAGEUP(len);
            return nullptr;
        }

        void free(void* base) {
            (void)base;
        }
    };

    inline _vma* kmem = nullptr;
};