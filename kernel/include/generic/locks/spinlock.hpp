
#include <cstdint>
#include <atomic>

#pragma once

namespace locks {
    class spinlock {
    private:
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
    public:
        spinlock() {

        }

        void lock() {
            while(flag.test_and_set(std::memory_order_acquire))
                __builtin_ia32_pause();
        }

        void unlock() {
            flag.clear(std::memory_order_release);
        }

        std::uint8_t test() {
            return flag.test(std::memory_order_acquire);
        }

    };
};