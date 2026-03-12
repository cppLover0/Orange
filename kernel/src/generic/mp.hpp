#pragma once
#include <cstdint>
#include <atomic>

namespace mp {

    struct barrier {
        std::atomic<uint32_t> count{0};
        std::atomic<uint32_t> generation{0};
    };

    void init();
    void sync();
    std::uint32_t cpu_count();
}