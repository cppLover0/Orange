#include <atomic>
#include <cstdint>

#include <generic/arch.hpp>

namespace locks {
    class spinlock {
    private:
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
    public:
            void lock() {
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
                return !flag.test_and_set(std::memory_order_acquire);
            }
    };
};