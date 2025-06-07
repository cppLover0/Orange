#define FLANTERM_IN_FLANTERM
#include <lib/flanterm/flanterm.h>
#include <lib/flanterm/backends/fb.h>
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
#include <drivers/pci/pci.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <other/assembly.hpp>
#include <uacpi/sleep.h>
#include <uacpi/event.h>
#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/acpi/acpi.hpp>
#include <generic/tty/tty.hpp>
#include <drivers/hpet/hpet.hpp>
#include <other/log.hpp>
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
#include <arch/x86_64/interrupts/syscalls/syscall.hpp>
#include <arch/x86_64/cpu/sse.hpp>
#include <generic/memory/vmm.hpp>
#include <drivers/xhci/xhci.hpp>

extern void (*__init_array[])();
extern void (*__init_array_end[])();

int initrd = 0;

void _1() {
    while(1) {
        //Serial::printf(" _1 ");
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

uint32_t default_fg = 0xFFFFFFFF;

int sec = 0;
int min = 0;

extern "C" void keyStub();
extern "C" uacpi_interrupt_ret handle_power_button(uacpi_handle ctx);

extern "C" void kmain() {

    for (size_t i = 0; &__init_array[i] != __init_array_end; i++) {
        __init_array[i]();
    }

    Serial::Init();

    Serial::printf("Serial initializied\n");
    LimineInfo info;

    sec = CMOS::Second();
    min = CMOS::Minute();

    struct flanterm_context *ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        (uint32_t*)info.fb_info->address, info.fb_info->width, info.fb_info->height, info.fb_info->pitch,
        info.fb_info->red_mask_size, info.fb_info->red_mask_shift,
        info.fb_info->green_mask_size, info.fb_info->green_mask_shift,
        info.fb_info->blue_mask_size, info.fb_info->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, &default_fg,
        NULL, &default_fg,
        NULL, 0, 0, 1,
        0, 0,
        0
    );

    //ft_ctx->set_text_fg_bright(ft_ctx,0xFFFFFFFF);
    //ft_ctx->set_text_fg_rgb(ft_ctx,0xFFFFFFFF);
    ft_ctx->cursor_enabled = 0;
    ft_ctx->clear(ft_ctx,1);

    LogInit((char*)ft_ctx);

    INFO("This is info message !\n");
    WARN("This is warning message !\n");
    ERROR("This is error message !\n");

    INFO("Bootloader: %s %s\n",info.bootloader_name,info.bootloader_version);
    INFO("RSDP: 0x%p\n",info.rsdp_address);
    INFO("HHDM: 0x%p\n",info.hhdm_offset);
    INFO("Kernel: Phys: 0x%p, Virt: 0x%p\n",info.ker_addr->physical_base,info.ker_addr->virtual_base);
    INFO("Framebuffer: Addr: 0x%p, Resolution: %dx%dx%d\n",info.fb_info->address,info.fb_info->width,info.fb_info->height,info.fb_info->bpp);
    INFO("Memmap: Start: 0x%p, Entry_count: %d\n",info.memmap->entries,info.memmap->entry_count);

    extern char is_ignored;

    is_ignored = 0;

    ft_ctx->cursor_enabled = 1;

    HHDM::applyHHDM(info.hhdm_offset);

    PMM::Init(info.memmap);
    INFO("PMM Initializied\n"); //it will be initializied anyway

    Paging::Init();
    INFO("Paging Initializied\n"); //it will be initializied anyway
    KHeap::Init();
    INFO("KHeap Initializied\n");
    
    cpudata_t* data = CpuData::Access();
    INFO("BSP CPU Data test: 1:0x%p 2:0x%p\n",data,CpuData::Access());

    GDT::Init();
    INFO("GDT Initializied\n");

    IDT::Init();
    INFO("IDT Initializied\n");

    ACPI::fullInit();
    INFO("ACPI Initializied\n");

    Lapic::Init();
    INFO("LAPIC Initializied\n");
    
    MP::Init();
    INFO("MP Initializied\n");

    PowerButton::Hook(handle_power_button);
    INFO("PowerButton initializied\n");

    VFS::Init();
    INFO("VFS Initializied\n");

    //tmpfs_dump();

    PS2Keyboard::Init(keyStub);

    SSE::Init();
    INFO("SSE Is enabled (or not) \n");
    
    Process::Init();
    INFO("Scheduling initializied\n");

    Syscall::Init();
    INFO("Syscall initializied\n");

    XHCI::Init();

    PCI::Init();
    INFO("PCI initializied\n");

    USTAR::ParseAndCopy();
    INFO("Loaded initrd\n");

    cpudata_t* cpu_data = CpuData::Access();

    uint64_t stack_5 = (uint64_t)PMM::VirtualBigAlloc(TSS_STACK_IN_PAGES); // for syscall
    Paging::alwaysMappedAdd(stack_5,TSS_STACK_IN_PAGES * PAGE_SIZE);
    cpu_data->kernel_stack = stack_5 + (TSS_STACK_IN_PAGES * PAGE_SIZE);
    cpu_data->user_stack = 0;

    VMM::Init(0);
    //VMM::Alloc(0,0,0);
    //__hlt();

    filestat_t stat;

    VFS::Stat("/usr/bin/initrd",(char*)&stat,1);

    char* elf = (char*)PMM::VirtualBigAlloc(CALIGNPAGEUP(stat.size,4096) / 4096);
    VFS::Read(elf,"/usr/bin/initrd",0);

    INFO("Loaded initrd !\n");

    //res.entry();

    initrd = Process::createProcess(0,0,1,0,0);

    for(int i = 0;i < 2;i++) {
        int proc = Process::createProcess((uint64_t)_1,0,0,0,0);
        Process::WakeUp(proc);
    }

    int serial = Process::createProcess((uint64_t)__serial_process_fetch,0,0,0,0);
    
    process_t* serial_proc = Process::ByID(serial);
    serial_proc->ctx.cr3 = HHDM::toPhys((uint64_t)Paging::KernelGet());  
    Process::WakeUp(serial);

    const char* pa = "/usr/bin/initrd";
    const char* ea = "=";

    char* initrd_argv[2];
    initrd_argv[0] = (char*)pa;
    initrd_argv[1] = 0;

    char* initrd_envp[2];
    initrd_envp[0] = (char*)ea;
    initrd_envp[1] = 0;

    Process::loadELFProcess(initrd,"/usr/bin/initrd",(uint8_t*)elf,initrd_argv,initrd_envp);

    process_t* initrd_proc = Process::ByID(initrd);

    Process::WakeUp(initrd);

    INFO("Waiting for interrupts...\n");

    TTY::Init();

    //__hlt();

    //MP::Sync();

    int res_sec = 0;
    if(CMOS::Minute() - min)
        res_sec *= (CMOS::Minute() - min);

    res_sec += CMOS::Second() - sec;

    INFO("Kernel is initializied for %d seconds\n",res_sec);

    __sti();

    while(1) {
        __hlt();
    }
    
}
