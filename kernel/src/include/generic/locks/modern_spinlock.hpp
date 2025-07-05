
#pragma once

#include <atomic>
#include <arch/x86_64/cpu/cache.hpp>

class Spinlock {
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:

    Spinlock() {

    }

    void lock() {
        while(flag.test_and_set(std::memory_order_acquire)) 
            __builtin_ia32_pause();
    }

    void wait() {
        while(flag.test(std::memory_order_acquire))
            __builtin_ia32_pause();
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }

};