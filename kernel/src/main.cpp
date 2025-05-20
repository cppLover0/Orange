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
#include <arch/x86_64/cpu/gdt.hpp>
#include <other/assembly.hpp>
#include <uacpi/sleep.h>
#include <uacpi/event.h>
#include <arch/x86_64/interrupts/idt.hpp>
#include <generic/acpi/acpi.hpp>
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

extern void (*__init_array[])();
extern void (*__init_array_end[])();

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
    ft_ctx->clear(ft_ctx,1);

    LogInit((char*)ft_ctx);

    Log(LOG_LEVEL_INFO,"This is info message !\n");
    Log(LOG_LEVEL_WARNING,"This is warning message !\n");
    Log(LOG_LEVEL_ERROR,"This is error message !\n");

    Log(LOG_LEVEL_INFO,"Bootloader: %s %s\n",info.bootloader_name,info.bootloader_version);
    Log(LOG_LEVEL_INFO,"RSDP: 0x%p\n",info.rsdp_address);
    Log(LOG_LEVEL_INFO,"HHDM: 0x%p\n",info.hhdm_offset);
    Log(LOG_LEVEL_INFO,"Kernel: Phys: 0x%p, Virt: 0x%p\n",info.ker_addr->physical_base,info.ker_addr->virtual_base);
    Log(LOG_LEVEL_INFO,"Framebuffer: Addr: 0x%p, Resolution: %dx%dx%d\n",info.fb_info->address,info.fb_info->width,info.fb_info->height,info.fb_info->bpp);
    Log(LOG_LEVEL_INFO,"Memmap: Start: 0x%p, Entry_count: %d\n",info.memmap->entries,info.memmap->entry_count);

    ft_ctx->cursor_enabled = 1;

    HHDM::applyHHDM(info.hhdm_offset);

    PMM::Init(info.memmap);
    Log(LOG_LEVEL_INFO,"PMM Initializied\n"); //it will be initializied anyway

    Paging::Init();
    Log(LOG_LEVEL_INFO,"Paging Initializied\n"); //it will be initializied anyway
    KHeap::Init();
    Log(LOG_LEVEL_INFO,"KHeap Initializied\n");
    
    cpudata_t* data = CpuData::Access();
    Log(LOG_LEVEL_INFO,"BSP CPU Data test: 1:0x%p 2:0x%p\n",data,CpuData::Access());

    GDT::Init();
    Log(LOG_LEVEL_INFO,"GDT Initializied\n");

    IDT::Init();
    Log(LOG_LEVEL_INFO,"IDT Initializied\n");

    ACPI::fullInit();
    Log(LOG_LEVEL_INFO,"ACPI Initializied\n");

    Lapic::Init();
    Log(LOG_LEVEL_INFO,"LAPIC Initializied\n");
    
    MP::Init();
    Log(LOG_LEVEL_INFO,"MP Initializied\n");

    PowerButton::Hook(handle_power_button);
    Log(LOG_LEVEL_INFO,"PowerButton initializied\n");

    VFS::Init();
    Log(LOG_LEVEL_INFO,"VFS Initializied\n");

    VFS::Create("/head",0);

    USTAR::ParseAndCopy();
    Log(LOG_LEVEL_INFO,"Loaded initrd\n");

    //tmpfs_dump();

    PS2Keyboard::Init(keyStub);
    
    Process::Init();
    Log(LOG_LEVEL_INFO,"Scheduling initializied\n");

    Syscall::Init();
    Log(LOG_LEVEL_INFO,"Syscall initializied\n");

    enable_sse();
    Log(LOG_LEVEL_INFO,"SSE Is enabled (or not) \n");

    cpudata_t* cpu_data = CpuData::Access();

    uint64_t stack_5 = (uint64_t)PMM::VirtualBigAlloc(TSS_STACK_IN_PAGES); // for syscall
    Paging::alwaysMappedAdd(stack_5,TSS_STACK_IN_PAGES * PAGE_SIZE);
    cpu_data->kernel_stack = stack_5 + (TSS_STACK_IN_PAGES * PAGE_SIZE);
    cpu_data->user_stack = 0;

    VMM::Init(0);
    //VMM::Alloc(0,0,0);
    //__hlt();

    Log(LOG_LEVEL_INFO,"Kernel is initializied !\n");

    filestat_t stat;

    VFS::Stat("/usr/bin/initrd",(char*)&stat);
    

    char* elf = (char*)PMM::VirtualBigAlloc(CALIGNPAGEUP(stat.size,4096) / 4096);
    VFS::Read(elf,"/usr/bin/initrd",0);

    Log(LOG_LEVEL_INFO,"Loaded initrd !\n");

    //res.entry();

    int initrd = Process::createProcess(0,0,1,0,0);

    for(int i = 0;i < 2;i++) {
        int proc = Process::createProcess((uint64_t)_1,0,0,0,0);
        Process::WakeUp(proc);
    }

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

    Log(LOG_LEVEL_INFO,"Initrd: stack_start: 0x%p, stack_end: 0x%p\n",initrd_proc->stack_start,initrd_proc->stack);

    Process::WakeUp(initrd);

    const char* str = "Hello, world from /dev/tty !\n";
    pAssert(VFS::Write((char*)str,"/dev/tty",String::strlen((char*)str),0) == 0,"devfs need to cry :(");

    Log(LOG_LEVEL_INFO,"Waiting for interrupts...\n");
 
    NLog("\033[2J \033[1;1H");
    //__hlt();

    //MP::Sync();

    __sti();

    while(1) {
        __hlt();
    }
    
}
