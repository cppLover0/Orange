#pragma once

#include <cstdint>

namespace utils {
    namespace flanterm {
        void init();
        void fullinit();
        void write(const char* buffer, std::size_t size);
    };
};