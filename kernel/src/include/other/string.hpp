
#pragma once

#include <stdint.h>
#include <drivers/serial/serial.hpp>

class String {
public:

    static uint64_t strlen(char* str);

    static char* itoa(uint64_t value, char* str, int base );

    // From meaty skeleton implementation
    static void* memset(void* bufptr, int value, uint64_t size);

    // From meaty skeleton implementation
    static void* memcpy(void* dstptr, const void* srcptr, uint64_t size);

    // From GNU Libc implementation
    static char* strtok(char *s, const char *delim);

};