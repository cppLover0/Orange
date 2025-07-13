
#include <cstdint>

#pragma once

#include <generic/mm/paging.hpp>
#include <drivers/tsc.hpp>

#include <etc/assembly.hpp>
#include <etc/etc.hpp>

namespace arch {
    namespace x86_64 {
        namespace cpu {
            class lapic {

                static inline std::uint64_t base() {
                    return (std::uint64_t)Other::toVirt(__rdmsr(0x1B) & 0xFFFFF000);
                }

                static inline std::uint32_t read(std::uint32_t reg) {
                    return *(volatile std::uint32_t*)(base() + reg); 
                }

                static inline void write(std::uint32_t reg,std::uint32_t value) {
                    *(volatile std::uint32_t*)(base() + reg) = value;
                }

            public:

                static inline std::uint32_t id() {
                    return read(0x20) >> 24;
                }

                static inline void eoi() {
                    write(0xB0,0);
                }

                static inline void init(std::uint32_t us) {
                    __wrmsr(0x1B,__rdmsr(0x1B));
                    memory::paging::kernelmap(0,__rdmsr(0x1B) & 0xFFFFF000);
                    write(0xf0,0xff | 0x100);
                    write(0x3e0,0x3);
                    write(0x320,32 | (1 << 16));
                    write(0x380,0xFFFFFFFF);
                    drivers::tsc::sleep(us);
                    std::uint64_t ticks = 0xFFFFFFFF - read(0x390);
                    write(0x320, 32 | (1 << 17));
                    write(0x3e0,0x3);
                    write(0x380,ticks);
                }
            };
        };
    };
};