
#include <arch/x86_64/interrupts/idt.hpp>
#include <stdint.h>
#include <drivers/serial/serial.hpp>
#include <other/assembly.hpp>

extern "C" void CPUKernelPanic(int_frame_t* frame) {
    Serial::printf("Kernel panic !\nVector: 0x%p, err_code: 0x%p\n",frame->vec,frame->err_code);
    __cli();
    __hlt();
}