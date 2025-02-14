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
#include <other/assembly.hpp>
#include <uacpi/sleep.h>
#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/acpi/acpi.hpp>
#include <drivers/hpet/hpet.hpp>
#include <other/log.hpp>
#include <lib/flanterm/flanterm.h>
#include <lib/flanterm/backends/fb.h>
#include <arch/x86_64/cpu/lapic.hpp>
#include <drivers/cmos/cmos.hpp>

extern void (*__init_array[])();
extern void (*__init_array_end[])();

void timer_test() {
    Log("Got timer interrupt cpu %d %d:%d:%d!\n",Lapic::ID(),CMOS::Hour(),CMOS::Minute(),CMOS::Second());
    Lapic::EOI();
    __sti();
    while(1) {
        __hlt();
    }
}

extern "C" void kmain() {

    for (size_t i = 0; &__init_array[i] != __init_array_end; i++) {
        __init_array[i]();
    }

    orange_status ret = 0;

    ret = Serial::Init();

    Serial::printf("Serial initializied\n");

    LimineInfo info;

    struct flanterm_context *ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        (uint32_t*)info.fb_info->address, info.fb_info->width, info.fb_info->height, info.fb_info->pitch,
        info.fb_info->red_mask_size, info.fb_info->red_mask_shift,
        info.fb_info->green_mask_size, info.fb_info->green_mask_shift,
        info.fb_info->blue_mask_size, info.fb_info->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );

    LogInit(ft_ctx);

    Log("Bootloader: %s %s\n",info.bootloader_name,info.bootloader_version);
    Log("RSDP: 0x%p\n",info.rsdp_address);
    Log("HHDM: 0x%p\n",info.hhdm_offset);
    Log("Kernel: Phys: 0x%p, Virt: 0x%p\n",info.ker_addr->physical_base,info.ker_addr->virtual_base);
    Log("Framebuffer: Addr: 0x%p, Resolution: %dx%dx%d\n",info.fb_info->address,info.fb_info->width,info.fb_info->height,info.fb_info->bpp);
    Log("Memmap: Start: 0x%p, Entry_count: %d\n",info.memmap->entries,info.memmap->entry_count);

    HHDM::applyHHDM(info.hhdm_offset);

    PMM::Init(info.memmap);
    Log("PMM Initializied\n"); //it will be initializied anyway

    Paging::Init();
    Log("Paging Initializied\n"); //it will be initializied anyway
    KHeap::Init();
    Log("KHeap Initializied\n");
    cpudata_t* data = CpuData::Access();
    Log("BSP CPU Data test: 1:0x%p 2:0x%p\n",data,CpuData::Access());

    GDT::Init();
    Log("GDT Initializied\n");

    IDT::Init();
    Log("IDT Initializied\n");

    ACPI::InitTables();
    Log("Early tables of ACPI initializied\n");

    HPET::Init();
    Log("HPET Initializied\n");
    
    ACPI::fullInit();
    Log("ACPI Initializied\n");

    Lapic::Init();
    Log("LAPIC Initializied\n");

    idt_entry_t* timer_entry = IDT::SetEntry(32,(void*)timer_test,0x8E);
    timer_entry->ist = 3;

    __sti();

    asm volatile("hlt");
    
}
