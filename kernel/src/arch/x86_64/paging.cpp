#include <cstdint>
#include <generic/arch.hpp>
#include <generic/bootloader/bootloader.hpp>
#include <generic/hhdm.hpp>
#include <generic/pmm.hpp>

#define PTE_MASK_VALUE_5 0x000ffffffffffff000
#define PTE_MASK_VALUE 0x000ffffffffff000
#define PTE_PRESENT (1 << 0)
#define PTE_RW (1 << 1)
#define PTE_USER (1 << 2)
#define PTE_WC ((1 << 7) | (1 << 3))
#define PTE_MMIO (1ull << 4)
#define LVL_PG_MASK (bootloader::bootloader->is_5_level_paging() ? PTE_MASK_VALUE_5 : PTE_MASK_VALUE)

int level_to_index(std::uintptr_t virt, int level) {
    if(bootloader::bootloader->is_5_level_paging()) {
        switch (level) {
            case 0:  return PTE_INDEX(virt, 48);
            case 1:  return PTE_INDEX(virt, 39);
            case 2:  return PTE_INDEX(virt, 30);
            case 3:  return PTE_INDEX(virt, 21);
            case 4:  return PTE_INDEX(virt, 12);
            default: return 0;
        }
    } else {
        switch (level) {
            case 0:  return PTE_INDEX(virt, 39);
            case 1:  return PTE_INDEX(virt, 30);
            case 2:  return PTE_INDEX(virt, 21);
            case 3:  return PTE_INDEX(virt, 12);
            default: return 0;
        }
    }
    return 0;
}

int64_t* __paging_next_level_noalloc(std::uint64_t* table, std::uint16_t idx) {
    if (!(table[idx] & PTE_PRESENT))
        return (int64_t*) -1;
    return (int64_t*) ((table[idx] & LVL_PG_MASK) + etc::hhdm());
}

std::int64_t __memory_paging_getphys(std::uint64_t* table, std::uint64_t virt, int level) {
    if (!table && (std::int64_t) table == -1)
        return -1;
    int max_level = bootloader::bootloader->is_5_level_paging() ? 4 : 3;
    if (max_level == level) {
        return (table[level_to_index(virt, level)] & PTE_PRESENT) ? table[level_to_index(virt, level)] & LVL_PG_MASK : -1;
    } else
        return __memory_paging_getphys((std::uint64_t*) __paging_next_level_noalloc(table, level_to_index(virt, level)), virt, level + 1);
    return -1;
}

uint64_t* x86_64_paging_next_level(std::uint64_t* table, std::uint16_t idx, std::uint64_t flags) {
    if (!(table[idx] & PTE_PRESENT))
        table[idx] = pmm::freelist::alloc_4k() | flags;
    return (uint64_t*) ((table[idx] & LVL_PG_MASK) + etc::hhdm());
}

std::uint64_t convert_flags(std::uint64_t flags) {
    std::uint64_t result = 0;
    if (flags & PAGING_NC)
        result |= PTE_MMIO;
    if (flags & PAGING_PRESENT)
        result |= PTE_PRESENT;
    if (flags & PAGING_RW)
        result |= PTE_RW;
    if (flags & PAGING_USER)
        result |= PTE_USER;
    if (flags & PAGING_WC)
        result |= PTE_WC;
    return result;
}

void x86_64_map_page(std::uintptr_t root, std::uintptr_t phys, std::uintptr_t virt, std::uint32_t flags) {
    std::uint64_t* cr30 = (std::uint64_t*) (root + etc::hhdm());
    std::uint64_t new_flags = PTE_PRESENT | PTE_RW;
    if (bootloader::bootloader->is_5_level_paging()) {
        if (PTE_INDEX(virt, 48) < 256)
            new_flags |= PTE_USER;
        uint64_t* pml4 = x86_64_paging_next_level(cr30, PTE_INDEX(virt, 48), new_flags);
        uint64_t* pml3 = x86_64_paging_next_level(pml4, PTE_INDEX(virt, 39), new_flags);
        uint64_t* pml2 = x86_64_paging_next_level(pml3, PTE_INDEX(virt, 30), new_flags);
        uint64_t* pml = x86_64_paging_next_level(pml2, PTE_INDEX(virt, 21), new_flags);
        pml[PTE_INDEX(virt, 12)] = phys | flags;
    } else {
        if (PTE_INDEX(virt, 39) < 256)
            new_flags |= PTE_USER;
        uint64_t* pml3 = x86_64_paging_next_level(cr30, PTE_INDEX(virt, 39), new_flags);
        uint64_t* pml2 = x86_64_paging_next_level(pml3, PTE_INDEX(virt, 30), new_flags);
        uint64_t* pml = x86_64_paging_next_level(pml2, PTE_INDEX(virt, 21), new_flags);
        pml[PTE_INDEX(virt, 12)] = phys | flags;
    }
}


namespace arch {
    [[gnu::weak]] void enable_paging(std::uintptr_t root) {
        asm volatile("mov %0, %%cr3" : : "r"(root) : "memory");
    }

    [[gnu::weak]] void map_page(std::uintptr_t root, std::uint64_t phys, std::uintptr_t virt, int flags) {
        x86_64_map_page(root,phys,virt,convert_flags(flags));
    }

    [[gnu::weak]] std::int64_t get_phys_from_page(std::uintptr_t root, std::uintptr_t virt) {
        return __memory_paging_getphys((std::uint64_t*)(root + etc::hhdm()),virt,0);
    }

    [[gnu::weak]] void destroy_root(std::uintptr_t root, int level) {
                std::uint64_t* table = (std::uint64_t*) (root + etc::hhdm());
        if (bootloader::bootloader->is_5_level_paging()) {
            if (level != 4) {
                if (level == 0) {
                    for (int i = 0; i < 256; i++) {
                        if (table[i] & PTE_PRESENT) {
                            destroy_root(table[i] & PTE_MASK_VALUE, level + 1);
                        }
                    }
                } else {
                    for (int i = 0; i < 512; i++) {
                        if (table[i] & PTE_PRESENT) {
                            destroy_root(table[i] & PTE_MASK_VALUE, level + 1);
                        }
                    }
                }
            }

            if (level != 0)
                pmm::freelist::free(root);
        } else {
            if (level != 3) {
                if (level == 0) {
                    for (int i = 0; i < 256; i++) {
                        if (table[i] & PTE_PRESENT) {
                            destroy_root(table[i] & PTE_MASK_VALUE, level + 1);
                        }
                    }
                } else {
                    for (int i = 0; i < 512; i++) {
                        if (table[i] & PTE_PRESENT) {
                            destroy_root(table[i] & PTE_MASK_VALUE, level + 1);
                        }
                    }
                }
            }

            if (level != 0)
                pmm::freelist::free(root);
        }
    }

    [[gnu::weak]] void copy_higher_half(std::uintptr_t root, std::uintptr_t src_root) {
        std::uint64_t* virt_rootcr3 = (std::uint64_t*) (root + etc::hhdm());
        std::uint64_t* virt_srccr3 = (std::uint64_t*) (src_root + etc::hhdm());
        for (int i = 255; i < 512; i++) {
            virt_rootcr3[i] = virt_srccr3[i];
        }
    }
};