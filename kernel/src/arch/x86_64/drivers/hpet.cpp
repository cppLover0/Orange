#include <cstdint>
#include <drivers/acpi.hpp>
#include <utils/gobject.hpp>
#include <generic/time.hpp>
#include <generic/arch.hpp>
#include <arch/x86_64/drivers/hpet.hpp>
#include <klibc/stdio.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/status.h>
#include <uacpi/acpi.h>
#include <generic/hhdm.hpp>
#include <generic/paging.hpp>

std::uint64_t hpet_base,hpet_clock_nano;
std::uint8_t hpet_is_32_bit = 0;

std::uint64_t __hpet_timestamp() {
    return hpet_is_32_bit ? *(volatile uint32_t*)(hpet_base + 0xf0) : *(volatile uint64_t*)(hpet_base + 0xf0);
}

void drivers::hpet::init() {
    uacpi_table hpet;
    uacpi_status ret = uacpi_table_find_by_signature("HPET",&hpet);
    if(ret != UACPI_STATUS_OK) {
        klibc::printf("HPET: hpet is not detected with status %d\r\n",ret);
        return; 
    }

    struct acpi_hpet* hpet_table = ((struct acpi_hpet*)hpet.virt_addr);
    hpet_base = (std::uint64_t)(hpet_table->address.address + etc::hhdm());
    paging::map_range(gobject::kernel_root,hpet_table->address.address,hpet_table->address.address + etc::hhdm(),PAGE_SIZE,PAGING_PRESENT | PAGING_RW);

    *(volatile std::uint64_t*)(hpet_base + 0x10) |= 1;
    hpet_is_32_bit = (*(volatile uint64_t*)hpet_base & (1 << 13)) ? 0 : 1;
    hpet_clock_nano = (*(volatile uint32_t*)(hpet_base + 4)) / 1000000;
    hpet_timer* new_hpet_inst = new hpet_timer;
    time::setup_timer(new_hpet_inst);
    klibc::printf("HPET: Detected %s hpet, current timestamp in nano %lli (hpet_base 0x%p)\r\n",hpet_is_32_bit ? "32 bit" : "64 bit", new_hpet_inst->current_nano(),hpet_base);
}

namespace drivers {
    void hpet_timer::sleep(std::uint64_t us) {
        std::uint64_t start = __hpet_timestamp();
        std::uint64_t conv = us * 1000;
        while((__hpet_timestamp() - start) * hpet_clock_nano < conv)
            asm volatile("nop");
    }


    std::uint64_t hpet_timer::current_nano() {
        return __hpet_timestamp() * hpet_clock_nano;
    }

    int hpet_timer::get_priority() {
        return 10;
    }
};