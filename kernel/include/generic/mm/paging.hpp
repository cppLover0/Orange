
#include <cstdint>
#include <assembly.hpp>

#define PTE_PRESENT (1 << 0)
#define PTE_RW (1 << 1)
#define PTE_USER (1 << 2)

namespace memory {
    
    void pat(std::uint8_t index, std::uint8_t type) {
        std::uint64_t pat = __rdmsr(0x277);
        pat &= ~(0xFFULL << (index * 8));
        pat |= ((std::uint64_t)type << (index * 8));
        __wrmsr(0x277, pat);
    }

    class paging {
    public:
        static void init();
        static void map(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t flags);
        static void maprange(std::uint64_t cr3,std::uint64_t phys,std::uint64_t virt,std::uint64_t len,std::uint64_t flags);
        static void mapentry(std::uint64_t cr3,std::uint8_t type);
        static void kernelmap(std::uint64_t cr3,std::uint64_t phys);
        static void enablekernel();
        static void enablepaging(std::uint64_t cr3);
        static void alwaysmappedadd(std::uint64_t phys, std::uint64_t len);
        static void alwaysmappedmap(std::uint64_t cr3);
    };

};