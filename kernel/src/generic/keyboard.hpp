#pragma once

#include <cstdint>

namespace keyboard {
    void init();
    void submit(std::uint8_t key);
}