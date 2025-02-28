
#include <uacpi/event.h>

#pragma once 

class PowerButton {
public:
    static void Hook( uacpi_interrupt_ret (*func)(uacpi_handle ctx));

};