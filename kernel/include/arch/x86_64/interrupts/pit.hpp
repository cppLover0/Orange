
#include <cstdint>

#pragma once

#define PIT_FREQUENCY 1193182

namespace arch {
    namespace x86_64 {
        namespace interrupts {
            class pit {
            public:
                static void init();
                static void sleep(std::uint32_t ms);
            };
        };
    };
};