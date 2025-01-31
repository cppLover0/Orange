
#include <stdint.h>

#pragma once

class PCI {
public:
    static uint32_t IN(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset,uint8_t bytewidth);

    static void OUT(uint8_t bus, uint8_t num, uint8_t function, uint8_t offset,uint32_t value,uint8_t bytewidth);
};