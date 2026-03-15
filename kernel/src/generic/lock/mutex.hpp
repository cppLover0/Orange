#pragma once
#include <cstdint>
#include <atomic>

#include <generic/lock/spinlock.hpp>
#include <generic/scheduling.hpp>

namespace locks {
    class mutex {
    private:
        std::atomic_flag flag = ATOMIC_FLAG_INIT;

    public:
        void lock() {
            if(locks::is_disabled)
                return;
                    
            while (flag.test_and_set(std::memory_order_acquire)) {
                process::yield();
            }
        }

        void unlock() {
            flag.clear(std::memory_order_release);
        }

        bool test() {
            return flag.test();
        }

        bool try_lock() {
            return !flag.test_and_set(std::memory_order_acquire);
        }
    };
};