#pragma once
#include <cstdint>
#include <generic/hhdm.hpp>
#define PIC1 0x20		
#define PIC2 0xA0		
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2+1)

namespace drivers {
    namespace ioapic {
        static inline void write(std::uint64_t base, std::uint32_t reg,std::uint32_t value) {
            std::uint64_t virt = (std::uint64_t)(base + etc::hhdm());
            *(volatile std::uint32_t*)virt = reg;
            *(volatile std::uint32_t*)(virt + 0x10) = value;
        }

        static inline std::uint32_t read(std::uint64_t base, std::uint32_t reg) {
            std::uint64_t virt = (std::uint64_t)(base + etc::hhdm());
            *(volatile std::uint32_t*)virt = reg;
            return *(volatile std::uint32_t*)(virt + 0x10);
        }

        void init();
        void set(std::uint8_t vec,std::uint8_t irq,std::uint64_t flags,std::uint64_t lapic);
        void mask(std::uint8_t irq);
        void unmask(std::uint8_t irq);

    };
};