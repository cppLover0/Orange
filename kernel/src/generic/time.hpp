#pragma once
#include <cstdint>
#include <klibc/stdio.hpp>
#include <atomic>

namespace time {
    class generic_timer {
    public:
        virtual int get_priority() = 0;

        virtual std::uint64_t current_nano() = 0;
        virtual void sleep(std::uint64_t us) = 0;
    };

    inline std::atomic<std::uint64_t> current_unix_time = 0;
    inline std::atomic<std::uint64_t> last_timestamp = 0;

    inline generic_timer* timer = nullptr;
    inline generic_timer* previous_timer = nullptr;

    inline std::uint64_t nano_remainder = 0; 

    inline void update_unix_time() {
        if (!timer) return;

        std::uint64_t now = timer->current_nano();
        
        if (last_timestamp != 0 && now > last_timestamp) {
            std::uint64_t delta_ns = now - last_timestamp;
            
            nano_remainder += delta_ns;

            if (nano_remainder >= 1'000'000'000) {
                current_unix_time += (nano_remainder / 1'000'000'000); 
                nano_remainder %= 1'000'000'000;                   
            }
        }

        last_timestamp = now;
    }


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