
#include <cstdint>
#include <etc/assembly.hpp>

#pragma once

#define PTE_MASK_VALUE 0x000ffffffffff000

#define PTE_PRESENT (1 << 0)
#define PTE_RW (1 << 1)
#define PTE_USER (1 << 2)
#define PTE_WC ((1 << 7) | (1 << 3))
#define PTE_MMIO (1ull << 4)

#define PTE_INDEX(address,bit) ((address & (uint64_t)0x1FF << bit) >> bit)

typedef struct alwaysmapped {
    std::uint64_t phys;
    std::uint64_t len;
    struct alwaysmapped* next;
} alwaysmapped_t;

namespace memory {
    
    inline void pat(std::uint8_t index, std::uint8_t type) {
        std::uint64_t pat = __rdmsr(0x277);
        pat &= ~(0xFFULL << (index * 8));
        pat |= ((std::uint64_t)type << (index * 8));
        __wrmsr(0x277, pat);
    }

    class paging {
    public:
        static void init();
        static void map(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t flags);
        static void mapid(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t flags,std::uint32_t id);
        static void maprange(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t len,std::uint64_t flags);
        static void zerorange(std::uint64_t cr3,std::uint64_t virt,std::uint64_t len);
        static void destroyrange(std::uint64_t cr3, std::uint64_t virt, std::uint64_t len);
        static void duplicaterangeifexists(std::uint64_t src_cr3, std::uint64_t dest_cr3, std::uint64_t virt, std::uint64_t len, std::uint64_t flags);
        static void maprangeid(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t len,std::uint64_t flags, std::uint32_t id);
        static void mapentry(std::uint64_t cr3,std::uint8_t type,std::uint64_t add_flags);
        static void mapkernel(std::uint64_t cr3,std::uint32_t id);
        static void* kernelmap(std::uint64_t cr3,std::uint64_t phys);
        static void enablekernel();
        static void enablepaging(std::uint64_t cr3);
        static void alwaysmappedadd(std::uint64_t phys, std::uint64_t len);
        static void alwaysmappedmap(std::uint64_t cr3,std::uint32_t id);
        static std::uint64_t kernelget();

        static void destroy(std::uint64_t cr3);

        static void change(std::uint64_t cr3, std::uint64_t virt, std::uint64_t flags);

        static void changerange(std::uint64_t cr3, std::uint64_t virt, std::uint64_t len , std::uint64_t flags);


    };

};