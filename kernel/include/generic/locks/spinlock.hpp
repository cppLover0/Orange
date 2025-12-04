
#include <cstdint>
#include <atomic>

#pragma once

#include <etc/logging.hpp>
#include <etc/assembly.hpp>

extern "C" void yield0();

namespace locks {
    class spinlock {
    private:
        volatile std::atomic_flag flag = ATOMIC_FLAG_INIT;
        char is_cli = 0;
    public:
        spinlock() {

        }

        void lock() {
            while(flag.test_and_set(std::memory_order_acquire)) {
                asm volatile("nop");
                
            }
        }

        void enable_scheduling_optimization() {
            is_cli = 1;
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