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
#include <generic/memory/heap.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/interrupts/idt.hpp>

extern void (*__init_array[])();
extern void (*__init_array_end[])();

extern "C" void kmain() {

    for (size_t i = 0; &__init_array[i] != __init_array_end; i++) {
        __init_array[i]();
    }

    orange_status ret = 0;

    ret = Serial::Init();

    Serial::printf("Serial initializied\n");

    LimineInfo info;

    Serial::printf("Bootloader: %s %s\n",info.bootloader_name,info.bootloader_version);
    Serial::printf("RSDP: 0x%p\n",info.rsdp_address);
    Serial::printf("HHDM: 0x%p\n",info.hhdm_offset);
    Serial::printf("Kernel: Phys: 0x%p, Virt: 0x%p\n",info.ker_addr->physical_base,info.ker_addr->virtual_base);
    Serial::printf("Framebuffer: Addr: 0x%p, Resolution: %dx%dx%d\n",info.fb_info->address,info.fb_info->width,info.fb_info->height,info.fb_info->bpp);
    Serial::printf("Memmap: Start: 0x%p, Entry_count: %d\n",info.memmap->entries,info.memmap->entry_count);

    HHDM::applyHHDM(info.hhdm_offset);

    PMM::Init(info.memmap);
    Serial::printf("PMM Initializied\n"); //it will be initializied anyway

    Paging::Init();
    Serial::printf("Paging Initializied\n"); //it will be initializied anyway
    KHeap::Init();
    Serial::printf("KHeap Initializied\n");
    cpudata_t* data = CpuData::Access();
    Serial::printf("BSP CPU Data test: 1:0x%p 2:0x%p\n",data,CpuData::Access());

    GDT::Init();
    Serial::printf("GDT Initializied\n");

    IDT::Init();
    Serial::printf("IDT Initializied\n");
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
