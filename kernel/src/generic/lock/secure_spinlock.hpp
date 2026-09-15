#pragma once
#include <generic/lock/process.hpp>
#include <atomic>
#include <cstdint>

#include <generic/time.hpp>

#include <utils/assert.hpp>
#include <generic/arch.hpp>

namespace locks {
    class secure_spinlock {
    private:

        std::uint64_t magic = 0x1234DEADDEAD;
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
        int last_pid = 0;
    public:

            void lock() {
                int id = process::id();

                std::uint64_t start = time::timer->current_nano();

                assert(arch::test_interrupts() == false, "bug");
                    
                while (flag.test_and_set(std::memory_order_acquire)) {

                    assert(magic == 0x1234DEADDEAD, "lcok corruption");

                    assert(id != last_pid, "deadlock detected");
                    assert((time::timer->current_nano() - start) < (5ull * 1000 * 1000 * 1000), "deadlock detected");

                    arch::pause();
                }

                process::lock();
                arch::memory_barrier();

            }

            void unlock() {

                assert(test() == true, "unlock")
                flag.clear(std::memory_order_release);

                process::unlock();
                arch::memory_barrier();

            }

            bool test() {
                return flag.test();
            }

            bool try_lock() {
                return flag.test_and_set(std::memory_order_acquire);
            }
    };
}