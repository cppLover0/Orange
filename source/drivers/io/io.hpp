
#pragma once

#include <stdint.h>

class IO {
public:

    static uint32_t IN(uint16_t port,uint8_t bytewidth);

    static void OUT(uint16_t port,uint32_t value,uint8_t bytewidth);

};

#include "io.cpp"