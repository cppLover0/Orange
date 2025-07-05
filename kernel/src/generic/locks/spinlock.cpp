

#include <stdbool.h>
#include <generic/locks/spinlock.hpp>
#include <stdatomic.h>

char is_ignored = 1;

void spinlock_lock(char* lock) {

    atomic_flag* mutex = (atomic_flag*)lock;

    while(atomic_flag_test_and_set(mutex))
        __builtin_ia32_pause();
}

void spinlock_unlock(char* lock) {
    atomic_flag* mutex = (atomic_flag*)lock;
    atomic_flag_clear(mutex);
}