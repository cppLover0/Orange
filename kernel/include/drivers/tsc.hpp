
#include <cstdint>
#include <arch/x86_64/interrupts/pit.hpp>

#pragma once

namespace drivers {
    class tsc {
    public:
        static void init();
        static void cpuinit();
        static void sleep(std::uint64_t us);
        static std::uint64_t currentnano();
        static std::uint64_t currentus();
    };
};