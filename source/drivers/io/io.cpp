
#include <stdint.h>

uint32_t IO::IN(uint16_t port,uint8_t bytewidth) {
    uint32_t result;
    uint16_t result16;
    uint8_t result8;
    switch(bytewidth) {
        case 1:
            asm volatile("inb %1,%0" : "=a"(result8) : "Nd"(port));
            return result8;
        case 2:
            asm volatile("inw %1,%0" : "=a"(result16) : "Nd"(port));
            return result16;
        case 4:
            asm volatile("inl %1,%0" : "=a"(result) : "Nd"(port));
            return result;
        default:
            return 0;
    }
}

void IO::OUT(uint16_t port,uint32_t value,uint8_t bytewidth) {
    uint8_t value8 = value;
    uint16_t value16 = value;
    switch (bytewidth)
    {
        case 1:
            asm volatile("outb %0,%1" : : "a"(value8), "Nd"(port));
            return;

        case 2:
            asm volatile("outw %0,%1" : : "a"(value16), "Nd"(port));
            return;
        
        case 4:
            asm volatile("outl %0,%1" : : "a"(value), "Nd"(port));
            return;
        
    }
}