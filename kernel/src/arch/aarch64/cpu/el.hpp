#pragma once
#include <cstdint>
namespace aarch64 {
    namespace el {

        struct int_frame {
            std::uint64_t ip;
            std::uint64_t sp;
            std::uint64_t flags;
            std::uint64_t x[32];
        };

        void init();
    };
};