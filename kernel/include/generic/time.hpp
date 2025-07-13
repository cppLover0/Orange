
/* It will connect all timers together */

#include <cstdint>

#pragma once

#define TSC_TIMER 0
#define KVM_TIMER 1

class time {
public:

    static std::uint64_t counter();

    static void sleep(std::uint64_t us);
};