#pragma once
#include <cstdint>

namespace klibc {
    int strlen(const char* str);
    void* memcpy(void *__restrict dest, const void *__restrict src, std::size_t n);
    void* memset(void *s, int c, std::size_t n);
    void* memmove(void *dest, const void *src, std::size_t n);
    int memcmp(const void *s1, const void *s2, std::size_t n);
    char* strtok(char **next,char *str, const char *delim);
    char* strchr(const char *s, int c);
    char* strrchr(const char* str, int ch);
    char* strcat(char* dest, const char* src);
    int strncmp(const char *s1, const char *s2, std::size_t n);
    int strcmp(const char *s1, const char *s2);
};