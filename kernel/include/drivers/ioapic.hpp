
#include <cstdint>

#pragma once

#include <etc/etc.hpp>

namespace drivers {
    class ioapic {
    public:
        
        static inline void write(std::uint64_t base, std::uint32_t reg,std::uint32_t value) {
            std::uint64_t virt = (std::uint64_t)Other::toVirt(base);
            *(volatile std::uint32_t*)virt = reg;
            *(volatile std::uint32_t*)(virt + 0x10) = value;
        }

        static inline std::uint32_t read(std::uint64_t base, std::uint32_t reg) {
            std::uint64_t virt = (std::uint64_t)Other::toVirt(base);
            *(volatile std::uint32_t*)virt = reg;
            return *(volatile std::uint32_t*)(virt + 0x10);
        }

        static void init();
        static void set(std::uint8_t vec,std::uint8_t irq,std::uint64_t flags,std::uint64_t lapic);

    };
};