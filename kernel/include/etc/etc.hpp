
#pragma once

#include <etc/bootloaderinfo.hpp>
#include <cstdint>

#define ALIGNUP(VALUE,c) ((VALUE + c - 1) & ~(c - 1))
#define ALIGNDOWN(VALUE,c) ((VALUE / c) * c)

class Other {
public:
    static void ConstructorsInit();

    static inline std::uint64_t toPhys(void* addr) {
        uint64_t hhdm = BootloaderInfo::AccessHHDM();
        return (uint64_t)addr - hhdm;
    }

    static inline void* toVirt(uint64_t phys) {
        uint64_t hhdm = BootloaderInfo::AccessHHDM();
        return (void*)(phys + hhdm);
    }

};