#include <cstdint>
#include <drivers/acpi.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/acpi.h>

#include <uacpi/types.h>
#include <uacpi/platform/arch_helpers.h>
#include <uacpi/kernel_api.h>
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <uacpi/event.h>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>

void acpi::init_tables() {
#if defined(__x86_64__)
    uacpi_status ret = uacpi_initialize(0);
    (void)ret;
#endif
}

void acpi::full_init() {
#if defined(__x86_64__)
    uacpi_namespace_load();
    uacpi_namespace_initialize();
    uacpi_finalize_gpe_initialization();
    uacpi_set_interrupt_model(UACPI_INTERRUPT_MODEL_IOAPIC);
#endif
}