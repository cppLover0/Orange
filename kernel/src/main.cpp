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
#include <uacpi/event.h>
#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/acpi/acpi.hpp>
#include <drivers/hpet/hpet.hpp>
#include <other/log.hpp>
#include <lib/flanterm/flanterm.h>
#include <lib/flanterm/backends/fb.h>
#include <arch/x86_64/cpu/lapic.hpp>
#include <drivers/cmos/cmos.hpp>
#include <generic/mp/mp.hpp>
#include <uacpi/utilities.h>
#include <arch/x86_64/interrupts/ioapic.hpp>
#include <other/assert.hpp>
#include <drivers/power_button/power_button.hpp>
#include <generic/VFS/vfs.hpp>
#include <generic/VFS/ustar.hpp>
#include <generic/VFS/tmpfs.hpp>
#include <generic/elf/elf.hpp>
#include <drivers/ps2keyboard/ps2keyboard.hpp>
#include <arch/x86_64/scheduling/scheduling.hpp>

extern void (*__init_array[])();
extern void (*__init_array_end[])();

void test() {
    Log("Hello, world from elf !");
    __sti();
    while(1) {
        __nop();
    }
}

void _1() {
    while(1) {
        __hlt();
    }
}

void _2() {
    while(1) {
        __cli();
        NLog("2");
        __sti();
        __hlt();
    }
}

extern "C" void keyStub();
extern "C" uacpi_interrupt_ret handle_power_button(uacpi_handle ctx);

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

    ft_ctx->set_text_fg_rgb(ft_ctx,0xFFFFFFFF);
    ft_ctx->cursor_enabled = 0;

    LogInit(ft_ctx);

    Log("Bootloader: %s %s\n",info.bootloader_name,info.bootloader_version);
    Log("RSDP: 0x%p\n",info.rsdp_address);
    Log("HHDM: 0x%p\n",info.hhdm_offset);
    Log("Kernel: Phys: 0x%p, Virt: 0x%p\n",info.ker_addr->physical_base,info.ker_addr->virtual_base);
    Log("Framebuffer: Addr: 0x%p, Resolution: %dx%dx%d\n",info.fb_info->address,info.fb_info->width,info.fb_info->height,info.fb_info->bpp);
    Log("Memmap: Start: 0x%p, Entry_count: %d\n",info.memmap->entries,info.memmap->entry_count);

    __wrmsr(0xC0000101,0); // write to zero cuz idk what can be in this msr

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

    idt_entry_t* elf_test1 = IDT::SetEntry(0x80,(void*)test,0xEE);

    ACPI::fullInit();
    Log("ACPI Initializied\n");

    Lapic::Init();
    Log("LAPIC Initializied\n");
    
    MP::Init();
    Log("MP Initializied\n");

    PowerButton::Hook(handle_power_button);
    Log("PowerButton initializied\n");

    VFS::Init();
    Log("VFS Initializied\n");

    VFS::Create("/head",0);

    USTAR::ParseAndCopy();
    Log("Loaded initrd\n");

    //tmpfs_dump();

    PS2Keyboard::Init(keyStub);
    
    Process::Init();
    Log("Scheduling initializied\n");

    Log("Kernel is initializied !\n");

    HPET::Sleep(1000 * 100);

    ft_ctx->clear(ft_ctx,1);

    Log("Waiting for interrupts...\n");

    int breakpoint = 0;

    filestat_t stat;

    VFS::Stat("/bin/initrd",(char*)&stat);

    char* elf = (char*)PMM::VirtualBigAlloc(CALIGNPAGEUP(stat.size,4096) / 4096);

    VFS::Read(elf,"/bin/initrd");

    ft_ctx->cursor_enabled = 1;

    //res.entry();

    int initrd = Process::createProcess(0,0,1,0);
    int _1i = Process::createProcess((uint64_t)_1,0,0,0);

    Process::loadELFProcess(initrd,(uint8_t*)elf,0,0);

    Process::WakeUp(initrd);
    Process::WakeUp(_1i);

    MP::Sync();

    __sti();

    while(1) {
        __hlt();
    }
    
}
