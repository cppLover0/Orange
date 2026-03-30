#include <cstdint>
#include <generic/bootloader/bootloader.hpp>
#include <utils/cxx/cxx_constructors.hpp>
#include <generic/arch.hpp>
#include <utils/flanterm.hpp>
#include <generic/pmm.hpp>
#include <generic/paging.hpp>
#include <generic/heap.hpp>
#include <drivers/acpi.hpp>
#include <generic/time.hpp>
#include <drivers/nvme.hpp>
#include <drivers/powerbutton.hpp>
#include <generic/mp.hpp>
#include <generic/vfs.hpp>
#include <drivers/xhci.hpp>

#if defined(__x86_64__)
#include <arch/x86_64/drivers/pci.hpp>
#include <arch/x86_64/drivers/serial.hpp>
#endif

extern std::size_t memory_size;
extern int is_early;

extern "C" void main() {
    utils::cxx::init_constructors();
    bootloader::init();
#if defined(__x86_64__)
    x86_64::serial::init();
#endif
    pmm::init();
    paging::init();
    kheap::init();
    utils::flanterm::init();
    utils::flanterm::fullinit();
    log("pmm", "total usable memory: %lli bytes", memory_size);
    log("paging", "enabled kernel root with %d level paging", arch::level_paging());
    log("kheap", "available memory %lli bytes", KHEAP_SIZE);
    acpi::init_tables();
    arch::init(ARCH_INIT_EARLY);
    acpi::full_init();
    is_early = 0;
    arch::init(ARCH_INIT_COMMON);
    mp::init();
    mp::sync();
    vfs::init();
    drivers::powerbutton::init();
    drivers::nvme::init();
    xhci_init();

#if defined(__x86_64__)
    x86_64::pci::initworkspace();
    log("pci", "launched all drivers");
#endif
    klibc::printf("Boot is done\r\n");
    mp::sync();
    arch::enable_interrupts();
    while(1) {
        //klibc::printf("current sec %lli\r\n",time::timer->current_nano() / (1000 * 1000 * 1000));
        arch::wait_for_interrupt();
    }
}
