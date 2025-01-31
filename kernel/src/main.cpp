#include <cstdint>
#include <cstddef>
#include <lib/limineA/limine.h>
#include <drivers/serial/serial.hpp>
#include <generic/status/status.hpp>
#include <generic/limineA/limineinfo.hpp>
#include <other/hhdm.hpp>
#include <other/string.hpp>
#include <generic/memory/pmm.hpp>
#include <generic/memory/paging.hpp>

extern void (*__init_array[])();
extern void (*__init_array_end[])();

extern "C" void kmain() {

    for (size_t i = 0; &__init_array[i] != __init_array_end; i++) {
        __init_array[i]();
    }

    orange_status ret = 0;

    ret = Serial::Init();

    Serial::printf("[%s] Serial initializied\n",ret >> 4 == 1 ? "+" : "-");

    LimineInfo info;

    Serial::printf("Bootloader: %s %s\n",info.bootloader_name,info.bootloader_version);
    Serial::printf("RSDP: 0x%p\n",info.rsdp_address);
    Serial::printf("HHDM: 0x%p\n",info.hhdm_offset);
    Serial::printf("Kernel: Phys: 0x%p, Virt: 0x%p\n",info.ker_addr->physical_base,info.ker_addr->virtual_base);
    Serial::printf("Framebuffer: Addr: 0x%p, Resolution: %dx%dx%d\n",info.fb_info->address,info.fb_info->width,info.fb_info->height,info.fb_info->bpp);
    Serial::printf("Memmap: Start: 0x%p, Entry_count: %d\n",info.memmap->entries,info.memmap->entry_count);

    HHDM::applyHHDM(info.hhdm_offset);

    Serial::printf("Initializing PMM\n");
    PMM::Init(info.memmap);

    Serial::printf("Initializing Paging\n");
    Paging::Init();

    Serial::printf("Starting PMM Test\n");

    void* f1 = PMM::VirtualAlloc();
    void* f2 = PMM::VirtualAlloc();
    void* f3 = PMM::VirtualBigAlloc(256);
    void* f4 = PMM::VirtualAlloc();
    PMM::VirtualFree(f1);
    void* f5 = PMM::VirtualAlloc();
    PMM::VirtualBigFree(f3,256);
    void* f6 = PMM::VirtualBigAlloc(255);
    void* f7 = PMM::VirtualAlloc();

    Serial::printf("PMM Test\nf1: 0x%p f2: 0x%p f3: 0x%p f4: 0x%p f5: 0x%p f6: 0x%p f7: 0x%p\n",f1,f2,f3,f4,f5,f6,f7);

    uint8_t t = 0;
    char m = 0;
    while(1) {
        String::memset(info.fb_info->address,t,info.fb_info->height * info.fb_info->pitch);
        if(t == 0xFF) {
            m = 1;
        } else if(t == 0) {
            m = 0;
        }
        if(m)
            t--;
        else
            t++;
        
    }

    asm volatile("hlt");
    
}
