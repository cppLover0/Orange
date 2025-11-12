
#pragma once

#include <etc/bootloaderinfo.hpp>
#include <cstdint>

#define ALIGNUP(VALUE,c) ((VALUE + c - 1) & ~(c - 1))
#define ALIGNDOWN(VALUE,c) ((VALUE / c) * c)

static inline uint64_t get_rflags() {
    uint64_t rflags;

    __asm__ volatile (
        "pushfq\n\t"
        "popq %0"
        : "=r"(rflags)
        :
        : "memory"
    );
    return rflags;
}

static inline int is_sti() {
    std::uint64_t eflags = get_rflags();
    return (eflags & (1 << 9)) != 0; 
}

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

    static inline std::uint64_t toPhys(std::uint64_t addr) {
        uint64_t hhdm = BootloaderInfo::AccessHHDM();
        return (uint64_t)addr - hhdm;
    }

};