
#include <cstdint>
#include <atomic>

#pragma once

namespace locks {
    class spinlock {
    private:
        volatile std::atomic_flag flag = ATOMIC_FLAG_INIT;
    public:
        spinlock() {

        }

        void lock() {
            while(flag.test_and_set(std::memory_order_acquire))
                asm volatile("nop");
        }

        std::uint8_t test_and_set() {
            return flag.test_and_set(std::memory_order_acquire);
        }

        void nowaitlock() {
            flag.test_and_set(std::memory_order_acquire);
        }

        void unlock() {
            flag.clear(std::memory_order_release);
        }

        std::uint8_t test() {
            return flag.test();
        }

    };
};