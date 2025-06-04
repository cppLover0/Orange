#include <uacpi/uacpi.h>
#include <stdint.h>
#include <generic/acpi/acpi.hpp>
#include <generic/memory/pmm.hpp>
#include <config.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <other/log.hpp>
#include <arch/x86_64/interrupts/ioapic.hpp>
#include <drivers/hpet/hpet.hpp>
#include <other/assert.hpp>
#include <uacpi/utilities.h>

// ill no use it
void ACPI::InitTables() {
    uacpi_status ret = uacpi_setup_early_table_access(PMM::VirtualBigAlloc(256),256 * PAGE_SIZE);
}

void ACPI::fullInit() {
    uacpi_status ret = uacpi_initialize(0);

    HPET::Init();
    INFO("HPET Initializied\n");

    IOAPIC::Init();
    INFO("IOAPIC Initializied\n");

    ret = uacpi_namespace_load();

    ret = uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);

    ret = uacpi_namespace_initialize();
    ret = uacpi_finalize_gpe_initialization();
}