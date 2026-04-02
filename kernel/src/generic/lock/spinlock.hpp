#pragma once
#include <atomic>
#include <cstdint>

#include <generic/arch.hpp>

namespace locks {
    inline bool is_disabled = 0;

    class preempt_spinlock {
    private:
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
    public:
            bool lock() {
                if(is_disabled)
                    return 0;

                //bool state = arch::test_interrupts();
                
                //arch::disable_interrupts();
                while (flag.test_and_set(std::memory_order_acquire)) {
                    arch::pause();
                }

                return false;
            }

            void unlock(bool state) {
                flag.clear(std::memory_order_release);

                if(state)
                    arch::enable_interrupts();
            }

            bool test() {
                return flag.test();
            }

            bool try_lock() {
                return !flag.test_and_set(std::memory_order_acquire);
            }
    };

    class spinlock {
    private:
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
    public:
            void lock() {
                if(is_disabled)
                    return;
                    
                while (flag.test_and_set(std::memory_order_acquire)) {
                    arch::pause();
                }
            }

            void unlock() {
                flag.clear(std::memory_order_release);
            }

            bool test() {
                return flag.test();
            }

            bool try_lock() {
                return flag.test_and_set(std::memory_order_acquire);
            }
    };
};