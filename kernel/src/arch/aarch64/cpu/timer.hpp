#pragma once
#include <cstdint>
#include <generic/time.hpp>

namespace aarch64 {
    namespace timer {
        class aarch64_timer final : public time::generic_timer {
        public:
            int get_priority() override;
            std::uint64_t current_nano() override;
            void sleep(std::uint64_t us) override;
        };

        void init();
    };
};