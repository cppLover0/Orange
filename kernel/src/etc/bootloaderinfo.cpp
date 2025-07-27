
#include <etc/bootloaderinfo.hpp>

#include <etc/logging.hpp>

#include <limine.h>
#include <cstdint>

namespace {

__attribute__((used, section(".limine_requests")))
volatile LIMINE_BASE_REVISION(3);

}

namespace {

__attribute__((used, section(".limine_requests")))
volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile limine_executable_address_request keraddr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile limine_module_request initrd_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
volatile LIMINE_MP(request) smp_request = {
    .id = LIMINE_MP_REQUEST,
    .revision = 0,
    .response = nullptr
};

}

namespace {

__attribute__((used, section(".limine_requests_start")))
volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile LIMINE_REQUESTS_END_MARKER;

}

std::uint64_t BootloaderInfo::AccessRSDP() {
    return rsdp_request.response->address;
}

struct limine_framebuffer* BootloaderInfo::AccessFramebuffer() {
    return framebuffer_request.response->framebuffers[0];
}

std::uint64_t BootloaderInfo::AccessHHDM() {
    return hhdm_request.response->offset;
}

struct limine_memmap_response* BootloaderInfo::AccessMemoryMap() {
    return memmap_request.response;
}

struct limine_executable_address_response* BootloaderInfo::AccessKernel() {
    return keraddr_request.response;
}

struct LIMINE_MP(response)* BootloaderInfo::AccessMP() {
    return smp_request.response;
}

struct limine_module_response* BootloaderInfo::AccessInitrd() {
    return initrd_request.response;
}