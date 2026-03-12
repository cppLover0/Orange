
#include <generic/bootloader/bootloader.hpp>
#include <generic/bootloader/limine.hpp>
#include <limine.h>

namespace {

__attribute__((used, section(".limine_requests")))
volatile std::uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

}

namespace {

__attribute__((used, section(".limine_requests")))
volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests"))) volatile limine_memmap_request memmap_request = { .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0, .response = nullptr };


__attribute__((used, section(".limine_requests"))) volatile limine_hhdm_request hhdm_request = { .id = LIMINE_HHDM_REQUEST_ID, .revision = 0, .response = nullptr };


__attribute__((used, section(".limine_requests"))) volatile limine_executable_address_request kaddr_request = { .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID, .revision = 0, .response = nullptr };

__attribute__((used, section(".limine_requests"))) volatile limine_mp_request mp_request = { .id = LIMINE_MP_REQUEST_ID, .revision = 0, .response = nullptr, .flags = 0};

__attribute__((used, section(".limine_requests"))) volatile limine_rsdp_request rsdp_request = { .id = LIMINE_RSDP_REQUEST_ID, .revision = 0, .response = nullptr };

#if defined(__x86_64__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    [[gnu::used]] [[gnu::section(".limine_requests")]] volatile limine_paging_mode_request _5lvl_paging = { .id = LIMINE_PAGING_MODE_REQUEST_ID,.revision = 0,.response = nullptr,.mode = LIMINE_PAGING_MODE_X86_64_5LVL,.max_mode = LIMINE_PAGING_MODE_AARCH64_5LVL,.min_mode = LIMINE_PAGING_MODE_AARCH64_4LVL };
#pragma GCC diagnostic pop
#endif
}

namespace {

__attribute__((used, section(".limine_requests_start")))
volatile std::uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile std::uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

}

namespace bootloader {

    std::uintptr_t limine::get_hhdm() {
        return hhdm_request.response->offset;
    }

    limine_framebuffer* limine::get_framebuffer() {
        return framebuffer_request.response->framebuffers[0];
    }

    void* limine::get_rsdp() {
        return rsdp_request.response->address;
    }

    std::uint64_t limine::get_kernel_phys() {
        return kaddr_request.response->physical_base;
    }

    std::uint64_t limine::get_kernel_virt() {
        return kaddr_request.response->virtual_base;
    }

    limine_memmap_response* limine::get_memory_map() {
        return memmap_request.response;
    }

    limine_mp_response* limine::get_mp_info() {
        return mp_request.response;
    }

#if defined(__x86_64__)
    bool limine::is_5_level_paging() {
        return _5lvl_paging.response->mode == LIMINE_PAGING_MODE_X86_64_5LVL ? true : false;
    }
#else
    bool limine::is_5_level_paging() {
        return false;
    }
#endif

};