
#include <stdint.h>
#include <lib/limineA/limine.h>

#pragma once

class LimineInfo {
public:

    struct limine_memmap_response* memmap;
    struct limine_executable_address_response* ker_addr;
    struct limine_framebuffer* fb_info;
    struct LIMINE_MP(response)* smp;
    uint64_t rsdp_address;
    uint64_t hhdm_offset;
    char* bootloader_name;
    char* bootloader_version;

    LimineInfo();


};