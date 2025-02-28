
#include <drivers/power_button/power_button.hpp>
#include <uacpi/event.h>
#include <uacpi/status.h>
#include <other/assert.hpp>
#include <other/log.hpp>
#include <stdint.h>

void PowerButton::Hook( uacpi_interrupt_ret (*func)(uacpi_handle ctx)) {
    uacpi_status ret = uacpi_install_fixed_event_handler(UACPI_FIXED_EVENT_POWER_BUTTON,func,UACPI_NULL);
    Assert(ret == UACPI_STATUS_OK,"Can't install power button !");
}
