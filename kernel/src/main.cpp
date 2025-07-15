
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/interrupts/irq.hpp>
#include <arch/x86_64/interrupts/pic.hpp>
#include <arch/x86_64/interrupts/pit.hpp>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/cpu/gdt.hpp>
#include <generic/mm/paging.hpp>
#include <drivers/kvmtimer.hpp>
#include <generic/vfs/vfs.hpp>
#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>
#include <drivers/serial.hpp>
#include <drivers/cmos.hpp>
#include <drivers/acpi.hpp>
#include <drivers/tsc.hpp>
#include <etc/assembly.hpp>
#include <etc/logging.hpp>
#include <etc/etc.hpp>

#include <uacpi/event.h>

#include <limine.h>

std::uint16_t KERNEL_GOOD_TIMER = 0;

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    Log::Display(LEVEL_MESSAGE_OK,"Got uacpi power_button !\n");
    return UACPI_INTERRUPT_HANDLED;
}

extern "C" void kmain() {
    
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

    Log::Display(LEVEL_MESSAGE_OK,"Everything is works !\n");

    uacpi_status ret = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
	    handle_power_button, UACPI_NULL
    );

    asm volatile("sti");
    while(1) {
        asm volatile("hlt");
    }

}
