
#pragma once

#include <cstdint>
#include <limine.h>

class BootloaderInfo {
public:
    static struct limine_framebuffer* AccessFramebuffer();
    static std::uint64_t AccessHHDM();
    static std::uint64_t AccessRSDP();
    static struct limine_memmap_response* AccessMemoryMap();
    static struct limine_executable_address_response* AccessKernel();
};