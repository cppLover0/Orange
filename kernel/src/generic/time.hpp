#pragma once
#include <cstdint>
#include <klibc/stdio.hpp>

namespace time {
    class generic_timer {
    public:
        virtual int get_priority() = 0;

        virtual std::uint64_t current_nano() = 0;
        virtual void sleep(std::uint64_t us) = 0;
    };

    inline generic_timer* timer = nullptr;
    inline generic_timer* previous_timer = nullptr;

    inline void setup_timer(generic_timer* ztimer) {
        if(!time::timer) {
            time::timer = ztimer;
        } else {
            if(time::timer->get_priority() < ztimer->get_priority()) {
                time::previous_timer = time::timer;
                time::timer = ztimer;
            }
        }
    }
}