
#include <cstdint>

#pragma once

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
    class kvmclock {
    public:
        static void init();
        static void sleep(std::uint64_t us);

        static std::uint64_t currentnano();
    };
};