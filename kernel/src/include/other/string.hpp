
#pragma once

#include <stdint.h>
#include <drivers/serial/serial.hpp>

class String {
public:

    static uint64_t strlen(char* str);

    static char* itoa(uint64_t value, char* str, int base );

    static uint64_t strncmp(const char* str1, const char* str2, uint64_t n);

    // From meaty skeleton implementation
    static void* memset(void* bufptr, int value, uint64_t size);

    // From meaty skeleton implementation
    static void* memcpy(void* dstptr, const void* srcptr, uint64_t size);

    static char* strtok(char *s, const char *delim);

    static char* strchr(const char *str, int ch);

    static char* strtok(char *str, const char *delim, char** saveptr);

    static int strcmp(const char *str1, const char *str2);

    static char* strncpy(char* dest, const char* src, uint64_t n);

    static char* strncat(char* dest, const char* src, uint64_t n);

    static char* strdup(const char* str);

};