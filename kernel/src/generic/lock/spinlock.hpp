#pragma once
#include <atomic>
#include <cstdint>
#include <klibc/stdio.hpp>
#include <utils/assert.hpp>

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
                
                assert(arch::test_interrupts() == false, "bug1");

                //arch::disable_interrupts();
                while (flag.test_and_set(std::memory_order_acquire)) {
                    arch::pause();
                }

                return false;
            }

            void unlock(bool state) {
                flag.clear(std::memory_order_release);

                arch::memory_barrier();

                assert(state == false, "bug4");
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

                assert(arch::test_interrupts() == false, "bug2");

                while (flag.test_and_set(std::memory_order_acquire)) {
                    arch::pause();
                }

                arch::memory_barrier();

            }

            void unlock() {
                flag.clear(std::memory_order_release);
                arch::memory_barrier();
            }

            bool test() {
                return flag.test();
            }

            bool try_lock() {
                return flag.test_and_set(std::memory_order_acquire);
            }
    };
};