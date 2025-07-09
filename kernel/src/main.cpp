
#include <generic/mm/heap.hpp>
#include <generic/mm/pmm.hpp>
#include <drivers/serial.hpp>

#include <etc/logging.hpp>
#include <etc/etc.hpp>

#include <limine.h>

extern "C" void kmain() {
    
    Other::ConstructorsInit();

    drivers::serial serial(DEFAULT_SERIAL_PORT);
    Log::Init();

    memory::pmm::_physical::init();
    Log::Display(LEVEL_MESSAGE_OK,"PMM Initializied\n");

    memory::heap::init();
    Log::Display(LEVEL_MESSAGE_OK,"Heap initializied\n");

    Log::Display(LEVEL_MESSAGE_INFO,"Heap Allocation Test 0x%p 0x%p 0x%p\n");
    for(int i = 0;i <= 50000;i ++) {
        Log::Display(LEVEL_MESSAGE_INFO,"Allocation %d address 0x%p size %d\n",i,memory::heap::malloc(16),123);
    }
    
    asm volatile("hlt");

}
