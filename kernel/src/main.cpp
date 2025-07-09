
#include <arch/x86_64/cpu/gdt.hpp>
#include <generic/mm/paging.hpp>
#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>
#include <drivers/serial.hpp>

#include <etc/assembly.hpp>
#include <etc/logging.hpp>
#include <etc/etc.hpp>

#include <limine.h>

extern "C" void kmain() {
    
    Other::ConstructorsInit();

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
    Log::Display(LEVEL_MESSAGE_FAIL,"GDT INIT... OK\n");

    Log::Display(LEVEL_MESSAGE_OK,"Everything is works !\n");
    
    asm volatile("hlt");

}
