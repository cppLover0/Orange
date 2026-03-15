#pragma once
#include <cstdint>
#include <generic/time.hpp>

struct pvclock_vcpu_time_info {
    std::uint32_t version;
    std::uint32_t pad0;
    std::uint64_t tsc_timestamp;
    std::uint64_t system_time;
    std::uint32_t tsc_to_system_mul;
    std::int8_t tsc_shift;
    std::uint8_t flags;
    std::uint8_t pad[2];
};

namespace drivers {

    class pvclock_timer final : public time::generic_timer {
    public:
        int get_priority() override;
        std::uint64_t current_nano() override;
        void sleep(std::uint64_t us) override;
    };

    class pvclock {
    public:
        static void init();
        static void bsp_init();
    };
};