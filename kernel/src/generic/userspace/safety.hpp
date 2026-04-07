#pragma once
#include <klibc/string.hpp>
#include <generic/hhdm.hpp>
#include <generic/scheduling.hpp>

inline static bool is_safe_to_rw(thread* proc, std::uint64_t mem, std::uint64_t len) {
    (void)proc;
    if(mem + len >= etc::hhdm())
        return false;
    return true;
}

inline static int safe_strlen(char* str, int max_len) {
    int len = 0;
    while (str[len] != '\0') {
        if(len > max_len)
            return max_len;
        len++;
    }
    return len;
}