
#include <cstdint>

#pragma once

namespace drivers {
    class hpet {
    public:
        static void init();
        static void sleep(std::uint64_t us);
    };
}