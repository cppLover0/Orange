
#include <generic/bootloader/bootloader.hpp>

namespace bootloader {
    class limine final : public bootloader_generic {
public:
        limine_framebuffer* get_framebuffer() override;
        std::uintptr_t get_hhdm() override;
        void* get_rsdp() override;
        std::uint64_t get_kernel_phys() override;
        std::uint64_t get_kernel_virt() override;
        limine_memmap_response* get_memory_map() override;
        limine_mp_response* get_mp_info() override;
        bool is_5_level_paging() override;
        limine_flanterm_fb_init_params_response* get_flanterm() override;
    };
};