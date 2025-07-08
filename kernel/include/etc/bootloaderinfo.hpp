
#pragma once

#include <cstdint>
#include <limine.h>

class BootloaderInfo {
public:
    static struct limine_framebuffer* AccessFramebuffer();
    static std::uint64_t AccessHHDM();
    static struct limine_memmap_response* AccessMemoryMap();
};