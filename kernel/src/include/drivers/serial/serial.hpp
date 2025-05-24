
#pragma once

#include <stdint.h>
#include <generic/status/status.hpp>
#include <drivers/io/io.hpp>

#define COM1 0x3F8

void __serial_process_fetch();

class Serial {
public:
    static void Init();
    static uint8_t Read();
    static void Write(uint8_t data);
    static void WriteArray(uint8_t* data,uint64_t len);
    static void WriteString(const char* str);
    static void printf(char* format, ...);
};
