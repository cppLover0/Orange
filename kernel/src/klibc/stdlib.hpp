#pragma once
#include <cstdint>

namespace klibc {
    void* malloc(std::size_t size);
    void free(void* ptr);
};