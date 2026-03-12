#pragma once
#include <arch/x86_64/cpu/lapic.hpp>

namespace x86_64 {
    namespace apic {
        static inline void eoi() {
            lapic::eoi();
        }
    };
};