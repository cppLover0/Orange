#include <cstdint>
#include <generic/arch.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <klibc/string.hpp>
#include <generic/bootloader/bootloader.hpp>
#include <utils/align.hpp>
#include <utils/gobject.hpp>
#include <klibc/stdio.hpp>
#include <generic/paging.hpp>

void __map_kernel(std::uintptr_t root) {
    extern std::uint64_t kernel_start;
    extern std::uint64_t kernel_end;
    std::uint64_t virt_base = bootloader::bootloader->get_kernel_virt();
    std::uint64_t phys_base = bootloader::bootloader->get_kernel_phys();

    for (std::uint64_t i = ALIGNDOWN((std::uint64_t) &kernel_start, 4096); i < ALIGNUP((std::uint64_t) &kernel_end, 4096); i += 4096) {
        paging::map_range(root, i - virt_base + phys_base, i, PAGE_SIZE, PAGING_PRESENT | PAGING_RW);
    }
}

namespace paging {

    void map_kernel(std::uintptr_t root) {
        return __map_kernel(root);
    }

    void mapentry(std::uintptr_t root, std::uint8_t type, std::uint32_t flags) {
        limine_memmap_response* mmap = bootloader::bootloader->get_memory_map();
        limine_memmap_entry* current = mmap->entries[0];
        for (uint64_t i = 0; i < mmap->entry_count; i++) {
            current = mmap->entries[i];
            if (current->type == type)
                paging::map_range(root, current->base, current->base + etc::hhdm(), current->length, PAGING_PRESENT | PAGING_RW | flags);
        }
    }


    void map_range(std::uintptr_t root, std::uint64_t phys, std::uintptr_t virt, std::size_t size, int flags) {
        for(std::size_t i = 0;i < size;i += PAGE_SIZE) {
            arch::map_page(root,phys + i, virt + i, flags);
        }
    }

    void zero_range(std::uintptr_t root, std::uintptr_t virt, std::size_t size) {
        for(std::size_t i = 0;i < size;i += PAGE_SIZE) {
            arch::map_page(root,0, virt + i, 0);
        }
    }

    void free_range(std::uintptr_t root, std::uintptr_t virt, std::uintptr_t len) {
        for (std::uint64_t i = 0; i < len; i += 4096) {
            std::int64_t phys = arch::get_phys_from_page(root, virt + i);
            if (phys != -1 && phys != 0) {
                pmm::freelist::free((std::uint64_t) phys);
                arch::map_page(root, 0, virt + i, 0);
            }
        }
    }

    void duplicate_range(std::uintptr_t root, std::uintptr_t src_root, std::uintptr_t virt, std::uintptr_t len, std::uint32_t flags) {
        zero_range(root, virt, len);
        for (std::uint64_t i = 0; i < len; i += 4096) {
            std::int64_t phys = arch::get_phys_from_page(src_root, virt + i);
            if (phys != -1 && phys != 0) {
                std::uint64_t new_phys = pmm::freelist::alloc_4k();
                klibc::memcpy((void*)(new_phys + etc::hhdm()), (void*) (phys + etc::hhdm()), 4096);
                arch::map_page(root, new_phys, virt + i, flags);
            }
        }
    }

    void init() {
        std::uintptr_t kernel_root = pmm::freelist::alloc_4k();
        gobject::kernel_root = kernel_root;
        mapentry(kernel_root, LIMINE_MEMMAP_USABLE, 0);
        mapentry(kernel_root, LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE, 0);
        mapentry(kernel_root, LIMINE_MEMMAP_FRAMEBUFFER, PAGING_WC);
        mapentry(kernel_root, LIMINE_MEMMAP_EXECUTABLE_AND_MODULES, 0);
        map_kernel(kernel_root);

        if(arch::fill_root) 
            arch::fill_root(kernel_root);

        arch::enable_paging(kernel_root);
        arch::tlb_flush(0,0);
        klibc::printf("Paging: Enabled kernel root with %d level paging\n\r", arch::level_paging());
    }
};
