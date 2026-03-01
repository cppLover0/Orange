#pragma once
#include <cstdint>
#include <generic/arch.hpp>
#include <generic/pmm.hpp>
#include <generic/hhdm.hpp>
#include <klibc/string.hpp>
#include <generic/bootloader/bootloader.hpp>
#include <utils/align.hpp>
#include <utils/gobject.hpp>
#include <klibc/stdio.hpp>

namespace paging {
    void map_kernel(std::uintptr_t root);
    void mapentry(std::uintptr_t root, std::uint8_t type, std::uint32_t flags);
    void map_range(std::uintptr_t root, std::uint64_t phys, std::uintptr_t virt, std::size_t size, int flags);
    void zero_range(std::uintptr_t root, std::uintptr_t virt, std::size_t size);
    void free_range(std::uintptr_t root, std::uintptr_t virt, std::uintptr_t len);
    void duplicate_range(std::uintptr_t root, std::uintptr_t src_root, std::uintptr_t virt, std::uintptr_t len, std::uint32_t flags);
    void init();
};
