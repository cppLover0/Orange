
#include <cstdint>
#include <generic/mm/pmm.hpp>
#include <etc/libc.hpp>

#pragma once

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

static inline std::uint64_t hash_fnv1(const char* key) {
    std::uint64_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (std::uint64_t)(unsigned char)(*p);
        hash *= FNV_PRIME;
    }
    return hash;
}

#include <cstddef>
#include <cstdint>
