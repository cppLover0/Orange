#pragma once
#include <cstdint>
#include <atomic>

#if defined(__x86_64__)
#include <arch/x86_64/assembly.hpp>
#endif

namespace random {
    uint64_t get_tick_count() {
#if defined(__x86_64__)
        return assembly::rdtsc();

 #elif defined(__aarch64__)
        uint64_t val;
        asm volatile("mrs %0, cntvct_el0" : "=r" (val));
        return val;

 #elif defined(__riscv)
        uint64_t val;
        asm volatile("rdcycle %0" : "=r" (val));
        return val;
#endif
    }

    uint64_t random() {
        std::atomic<std::uint64_t> state = get_tick_count();
        uint64_t current = state.fetch_add(0x9E3779B97F4A7C15, std::memory_order_relaxed);
        current = (current ^ (current >> 30)) * 0xBF58476D1CE4E5B9;
        current = (current ^ (current >> 27)) * 0x94D049BB133111EB;
        return current ^ (current >> 31);
    }
}