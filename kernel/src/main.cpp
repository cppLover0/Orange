
#include <arch/x86_64/syscalls/syscalls.hpp>
#include <arch/x86_64/syscalls/sockets.hpp>
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/interrupts/irq.hpp>
#include <arch/x86_64/interrupts/pic.hpp>
#include <arch/x86_64/interrupts/pit.hpp>
#include <arch/x86_64/scheduling.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <arch/x86_64/cpu/smp.hpp>
#include <arch/x86_64/cpu/sse.hpp>
#include <generic/mm/paging.hpp>
#include <generic/vfs/ustar.hpp>
#include <drivers/kvmtimer.hpp>
#include <generic/vfs/vfs.hpp>
#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>
#include <drivers/serial.hpp>
#include <drivers/cmos.hpp>
#include <drivers/acpi.hpp>
#include <etc/assembly.hpp>
#include <drivers/tsc.hpp>
#include <etc/logging.hpp>
#include <uacpi/event.h>
#include <etc/etc.hpp>
#include <limine.h>

#include <generic/locks/spinlock.hpp>

std::uint16_t KERNEL_GOOD_TIMER = HPET_TIMER;

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    Log::Display(LEVEL_MESSAGE_OK,"Got uacpi power_button !\n");
    return UACPI_INTERRUPT_HANDLED;
}

extern "C" void main() {
    
    Other::ConstructorsInit();
    asm volatile("cli");

    drivers::serial serial(DEFAULT_SERIAL_PORT);
    Log::Init();

    __wrmsr(0xC0000101,0);

    memory::pmm::_physical::init();
    Log::Display(LEVEL_MESSAGE_OK,"PMM Initializied\n");

    memory::heap::init();
    Log::Display(LEVEL_MESSAGE_OK,"Heap initializied\n");

    memory::paging::init();
    Log::Display(LEVEL_MESSAGE_OK,"Paging initializied\n");

    arch::x86_64::cpu::gdt::init();
    Log::Display(LEVEL_MESSAGE_OK,"GDT initializied\n");

    arch::x86_64::interrupts::idt::init();
    Log::Display(LEVEL_MESSAGE_OK,"IDT initializied\n");

    arch::x86_64::interrupts::pic::init();
    Log::Display(LEVEL_MESSAGE_OK,"PIC initializied\n");

    drivers::kvmclock::init();
    drivers::acpi::init();
    Log::Display(LEVEL_MESSAGE_OK,"ACPI initializied\n");
    
    vfs::vfs::init();
    Log::Display(LEVEL_MESSAGE_OK,"VFS initializied\n");

    Log::Display(LEVEL_MESSAGE_INFO,"Loading initrd\n");
    vfs::ustar::copy();
    Log::Display(LEVEL_MESSAGE_OK,"USTAR parsed\n");

    arch::x86_64::cpu::sse::init();
    Log::Display(LEVEL_MESSAGE_OK,"SSE initializied\n");

    arch::x86_64::cpu::mp::init();
    arch::x86_64::cpu::mp::sync(0);
    Log::Display(LEVEL_MESSAGE_OK,"SMP initializied\n");

    arch::x86_64::scheduling::init();
    Log::Display(LEVEL_MESSAGE_OK,"Scheduling initializied\n");

    arch::x86_64::syscall::init();
    Log::Display(LEVEL_MESSAGE_OK,"Syscalls initializied\n");

    sockets::init();
    Log::Display(LEVEL_MESSAGE_OK,"Sockets initializied\n");

    uacpi_status ret = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
	    handle_power_button, UACPI_NULL
    );

    char* argv[] = {0};
    char* envp[] = {"TERM=linux",0};

    Log::Display(LEVEL_MESSAGE_INFO,"Trying to sync cpus...\n");
    arch::x86_64::cpu::mp::sync(1);

    drivers::tsc::sleep(50000);

    arch::x86_64::process_t* init = arch::x86_64::scheduling::create();
    arch::x86_64::scheduling::loadelf(init,"/usr/bin/init",argv,envp,0);
    arch::x86_64::scheduling::wakeup(init);

    extern locks::spinlock* vfs_lock;
    extern locks::spinlock pmm_lock;

    Log::Display(LEVEL_MESSAGE_FAIL,"\e[1;1H\e[2J");
    arch::x86_64::cpu::lapic::tick(arch::x86_64::cpu::data()->lapic_block);

    dmesg("Now we are in userspace...");

    setwp();

    asm volatile("sti");
    while(1) {
        
        asm volatile("hlt");
    }

}
