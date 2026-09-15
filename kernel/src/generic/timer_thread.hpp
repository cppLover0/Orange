#pragma once

#include <cstdint>
#include <generic/lock/spinlock.hpp>
#include <generic/time.hpp>
#include <generic/scheduling.hpp>

namespace timer_thread {
    
    struct timer_thread_guest {
        std::uint64_t counter;
        std::uint64_t interval;
        std::uint64_t ownership;
        
        void* ctx;
        void (*callback)(void*);

        bool is_used;
        timer_thread_guest* next;
    };

    timer_thread_guest* create(void (*callback)(void*), void* ctx, std::uint64_t counter, std::uint64_t interval, std::uint64_t ownership);
    void remove(timer_thread_guest* guest);

    void init();
    
    inline timer_thread_guest* head = nullptr;
    inline thread* timer = nullptr;
    inline locks::spinlock lock;
};