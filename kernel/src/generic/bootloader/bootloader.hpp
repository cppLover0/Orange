#pragma once
#include <cstdint>
#include <limine.h>

namespace bootloader {

    class bootloader_generic {
    public:
        virtual limine_framebuffer* get_framebuffer() = 0;
        virtual std::uintptr_t get_hhdm() = 0;
        virtual void* get_rsdp() = 0;
        virtual std::uint64_t get_kernel_phys() = 0;
        virtual std::uintptr_t get_kernel_virt() = 0;
        virtual limine_memmap_response* get_memory_map() = 0;
        virtual limine_mp_response* get_mp_info() = 0;
        virtual bool is_5_level_paging() = 0;
        virtual limine_flanterm_fb_init_params_response* get_flanterm() = 0;
    };

    void init();
    inline bootloader_generic* bootloader = nullptr;
}