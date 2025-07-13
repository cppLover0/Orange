
#include <cstdint>
#include <drivers/hpet.hpp>

#include <arch/x86_64/interrupts/pit.hpp>

std::uint64_t hpet_base,hpet_clock_nano;
std::uint8_t hpet_is_32_bit = 0;

#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/status.h>
#include <uacpi/acpi.h>

#include <etc/logging.hpp>

#include <generic/mm/paging.hpp>

#include <etc/etc.hpp>

std::uint64_t __hpet_counter() {
    return hpet_is_32_bit ? *(volatile uint32_t*)(hpet_base + 0xf0) : *(volatile uint64_t*)(hpet_base + 0xf0);
}

void drivers::hpet::init() {
    uacpi_table hpet;
    uacpi_status ret = uacpi_table_find_by_signature("HPET",&hpet);
    if(ret != UACPI_STATUS_OK) {
        Log::Display(LEVEL_MESSAGE_FAIL,"HPET is disabled, Orange requires it to work\n");
        while(1) { asm volatile("hlt"); }
    }
    struct acpi_hpet* hpet_table = ((struct acpi_hpet*)hpet.virt_addr);
    hpet_base = (std::uint64_t)Other::toVirt(hpet_table->address.address);
    memory::paging::kernelmap(0,hpet_table->address.address);

    *(volatile std::uint64_t*)(hpet_base + 0x10) |= 1;
    hpet_is_32_bit = (*(volatile uint64_t*)hpet_base & (1 << 13)) ? 0 : 1;
    hpet_clock_nano = (*(volatile uint32_t*)(hpet_base + 4)) / 1000000;

}

void drivers::hpet::sleep(std::uint64_t us) {
    std::uint64_t start = __hpet_counter();
    std::uint64_t conv = us * 1000;
    while((__hpet_counter() - start) * hpet_clock_nano < conv)
        asm volatile("nop");
}
