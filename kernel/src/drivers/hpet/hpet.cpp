
#include <stdint.h>
#include <drivers/hpet/hpet.hpp>
#include <generic/memory/paging.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/status.h>
#include <drivers/serial/serial.hpp>
#include <other/assembly.hpp>
#include <other/hhdm.hpp>
#include <other/log.hpp>
#include <uacpi/acpi.h>
#include <other/assert.hpp>

char hpet_is_32_bit;
uint64_t hpet_base;
uint32_t hpet_clock_period;
uint32_t hpet_clock_period_nano;

void HPET::Init() {
    uacpi_table hpet;
    uacpi_status ret = uacpi_table_find_by_signature("HPET",&hpet);
    pAssert(ret == UACPI_STATUS_OK,"HPET is disabled");
    struct acpi_hpet* hpet_table = ((struct acpi_hpet*)hpet.virt_addr);
    hpet_base = HHDM::toVirt(hpet_table->address.address);
    Paging::KernelMap(hpet_table->address.address);
    *(volatile uint64_t*)(hpet_base + 0x10) |= 1;
    hpet_is_32_bit = (*(volatile uint64_t*)hpet_base & (1 << 13)) ? 0 : 1;
    hpet_clock_period = *(volatile uint32_t*)(hpet_base + 4);
    hpet_clock_period_nano = hpet_clock_period / 1000000;
    INFO("%s HPET: 0x%p\n",hpet_is_32_bit ? "32 Bit" : "64 Bit",hpet_base);
}

uint64_t hpet_counter() {
    return hpet_is_32_bit ? *(volatile uint32_t*)(hpet_base + 0xf0) : *(volatile uint64_t*)(hpet_base + 0xf0);
}

uint64_t HPET::NanoCurrent() {
    return hpet_counter() * hpet_clock_period_nano;
}

void HPET::Sleep(uint64_t usec) {
    uint64_t start = hpet_counter();
    uint64_t nanoseconds = usec * 1000; 
    while((hpet_counter() - start) * hpet_clock_period_nano < nanoseconds) { 
        __nop();
    }
}
