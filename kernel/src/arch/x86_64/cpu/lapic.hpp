
#include <cstdint>

#pragma once

#include <generic/paging.hpp>
#include <generic/time.hpp>
#include <generic/hhdm.hpp>
#include <utils/gobject.hpp>
#include <arch/x86_64/assembly.hpp>
#include <klibc/stdio.hpp>
namespace x86_64 {

        inline int is_lapic_init = 0;
        inline int is_x2apic = 0;

        class lapic {

                static inline std::uint64_t base() {
                    return (std::uint64_t)((assembly::rdmsr(0x1B) & 0xFFFFF000) + etc::hhdm());
                }

                static inline std::uint32_t read(std::uint32_t reg) {
                    if(is_x2apic) {
                        return assembly::rdmsr(0x800 + (reg >> 4));
                    } else {
                        return *(volatile std::uint32_t*)(base() + reg); 
                    }
                }

                static inline void write(std::uint32_t reg,std::uint32_t value) {
                    if(is_x2apic) {
                        assembly::wrmsr(0x800 + (reg >> 4), value);
                    } else {
                        *(volatile std::uint32_t*)(base() + reg) = value;
                    }
                }

            public:

                static std::uint32_t id();
                static void eoi();
                static void off();
                static void tick(std::uint64_t tick);
                static std::uint64_t init(std::uint32_t us);
            };
};