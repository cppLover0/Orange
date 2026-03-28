#include <cstdint>
#include <drivers/powerbutton.hpp>
#include <klibc/stdio.hpp>
#if defined(__x86_64__)
#include <uacpi/event.h>
#include <generic/poweroff.hpp>

static uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    (void)ctx;
    poweroff::reboot();
    return UACPI_INTERRUPT_HANDLED;
}

void drivers::powerbutton::init() {
    uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
	    handle_power_button, UACPI_NULL
    );
    log("powerbutton", "registered powerbutton at ip 0x%p",(std::uint64_t)handle_power_button);
}

#else

void drivers::powerbutton::init() {

}

#endif