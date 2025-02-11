
#pragma once

#include <stdint.h>

typedef struct {
    uint64_t base;
    uint64_t len;
} __attribute__((packed)) uacpi_io_struct_t;

class IO {
public:

    static uint32_t IN(uint16_t port,uint8_t bytewidth);

    static void OUT(uint16_t port,uint32_t value,uint8_t bytewidth);

};
