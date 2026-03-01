#pragma once
#include <cstdint>
#include <generic/time.hpp>

namespace drivers {

    class tsc_timer final : public time::generic_timer {
    public:
        int get_priority() override;
        std::uint64_t current_nano() override;
        void sleep(std::uint64_t us) override;
    };

    class tsc {
    public:
        static void init();
        static void init_bsp();
    };
};