
#include <stdint.h>
#include <other/assembly.hpp>

#pragma once

class CacheObject {
private:
    uint32_t cache_size = 0;
public:
    CacheObject() {
        uint32_t a,b,c,d;
        __cpuid(0x80000006,0,&a,&b,&c,&d);
        cache_size = c & 0xFF;
    }

    inline void FlushAll() {
        asm volatile("wbinvd");
    }

    inline void Flush(const void* addr, uint64_t size) {
        const char* ptr = static_cast<const char*>(addr);
        const char* end = ptr + size;
        for (; ptr < end; ptr += cache_size) {
            asm volatile("clflush %0" : : "m"(*ptr));
        }
        asm volatile("mfence" ::: "memory");
    }

    inline void FlushLine(const void* addr) {
        asm volatile("clflush %0" : : "m"(*(const char*)addr) : "memory");
        asm volatile("mfence" ::: "memory");
    }

};