
#include <generic/locks/spinlock.hpp>

char is_ignored = 1;

void spinlock_lock(char* lock) {

    if(is_ignored)
        return;

    while(__sync_lock_test_and_set(lock,1))
        __builtin_ia32_pause();
}

void spinlock_unlock(char* lock) {
    __sync_lock_release(lock);
    *lock = 0;
}