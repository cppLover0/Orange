#pragma once

#include <atomic>
#include <cstdint>

namespace utils {
    template <typename T>
    class number_allocator {
    public:
        std::atomic<T> ptr = 0;

        T allocate() {
            return ptr.fetch_add(1);
        }
    };
};