#include <uacpi/uacpi.h>
#include <stdint.h>
#include <generic/acpi/acpi.hpp>
#include <generic/memory/pmm.hpp>
#include <config.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>

void ACPI::InitTables() {
    uacpi_status ret = uacpi_setup_early_table_access(PMM::VirtualBigAlloc(256),256 * PAGE_SIZE);
}

void ACPI::fullInit() {
    uacpi_status ret = uacpi_initialize(0);
    ret = uacpi_namespace_load();
    ret = uacpi_namespace_initialize();
    ret = uacpi_finalize_gpe_initialization();
}