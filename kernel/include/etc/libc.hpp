
// simple libc basic memory implementations

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <generic/mm/heap.hpp>

inline void *memcpy(void *__restrict dest, const void *__restrict src, size_t n) {
    uint8_t *__restrict pdest = static_cast<uint8_t *__restrict>(dest);
    const uint8_t *__restrict psrc = static_cast<const uint8_t *__restrict>(src);

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

inline void *memset(void *s, int c, size_t n) {
    uint8_t *p = static_cast<uint8_t *>(s);

    for (size_t i = 0; i < n; i++) {
        p[i] = static_cast<uint8_t>(c);
    }

    return s;
}

inline void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = static_cast<uint8_t *>(dest);
    const uint8_t *psrc = static_cast<const uint8_t *>(src);

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

inline int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = static_cast<const uint8_t *>(s1);
    const uint8_t *p2 = static_cast<const uint8_t *>(s2);

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

inline int strlen(const char* str) {
    int idx = 0;
    while(str[idx])
        idx++;
    return idx;
}

inline void* malloc(size_t size) {
    return memory::heap::malloc(size);
}

inline void free(void* p) {
    memory::heap::free(p);
}

inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

inline int strncmp(const char *s1, const char *s2, size_t n) {
    size_t i = 0;
    while (i < n && s1[i] && (s1[i] == s2[i])) {
        i++;
    }
    if (i == n) return 0;
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

inline char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return NULL;
}

inline static char *strtok(char *str, const char *delim) {
    static char *next = NULL;
    if (str) next = str;
    if (!next) return NULL;

    char *start = next;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    if (!*start) {
        next = NULL;
        return NULL;
    }

    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }

    if (*end) {
        *end = '\0';
        next = end + 1;
    } else {
        next = NULL;
    }

    return start;
}

inline char* strdup(const char *s) {
    size_t len = 0;
    while (s[len]) len++;

    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;

    for (size_t i = 0; i <= len; i++) {
        copy[i] = s[i];
    }
    return copy;
}
