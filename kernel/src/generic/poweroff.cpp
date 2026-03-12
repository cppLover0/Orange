#include <cstdint>
#include <generic/poweroff.hpp>
#include <drivers/nvme.hpp>
#if defined(__x86_64__)
#include <uacpi/sleep.h>
#include <arch/x86_64/cpu/lapic.hpp>
#include <arch/x86_64/schedule_timer.hpp>
#endif
#include <generic/arch.hpp>
#include <klibc/stdio.hpp>
#include <generic/time.hpp>
#include <generic/lock/spinlock.hpp>

void poweroff::prepare_for_shutdown() {
    drivers::nvme::disable();
    klibc::printf("Poweroff: NVME is disabled\r\n");
#if defined(__x86_64__)
    x86_64::schedule_timer::off();
#endif
    locks::is_disabled = true;
    klibc::printf("Poweroff: Preparing for shutdown is successful\r\n");
}

void poweroff::off() {
    arch::disable_interrupts();
    prepare_for_shutdown();
    if(time::timer) {
        klibc::printf("Poweroff: Shutdowning after 3 seconds\r\n");
        time::timer->sleep(3 * (1000 * 1000));
    }
#if defined(__x86_64__) 
    uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
#endif
    klibc::printf("uhh its safe to shutdown yk\r\n");
    arch::hcf();
}

void poweroff::reboot() {
    arch::disable_interrupts();
    prepare_for_shutdown();
    if(time::timer) {
        klibc::printf("Poweroff: Rebooting after 3 seconds\r\n");
        time::timer->sleep(3 * (1000 * 1000));
    }
#if defined(__x86_64__) 
    uacpi_reboot();
#endif
    klibc::printf("uhh its safe to reboot yk\r\n");
    arch::hcf();
}