#include <cstdint>
#include <generic/bootloader/bootloader.hpp>
#include <utils/cxx/cxx_constructors.hpp>
#include <generic/arch.hpp>
#include <utils/flanterm.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/heap.hpp>
#include <drivers/acpi.hpp>

extern std::size_t memory_size;

extern "C" void kmain() {
    utils::cxx::init_constructors();
    bootloader::init();
    utils::flanterm::init();
    pmm::init();
    paging::init();
    kheap::init();
    utils::flanterm::fullinit();
    klibc::printf("PMM: Total usable memory: %lli bytes\r\n",memory_size); // i dont want to forgot these messages 
    klibc::printf("Paging: Enabled kernel root with %d level paging\n\r", arch::level_paging());
    klibc::printf("KHeap: Available memory %lli bytes\r\n",KHEAP_SIZE);
    acpi::init_tables();
    arch::init(ARCH_INIT_EARLY);
    acpi::full_init();
    arch::init(ARCH_INIT_COMMON);
    extern int is_early;
    is_early = 0;
    klibc::printf("Boot is done\r\n");
    //*(int*)0x100 = 0;
    arch::hcf();
}
