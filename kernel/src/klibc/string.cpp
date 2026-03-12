
#include <cstdint>
#include <klibc/string.hpp>

void* klibc::memcpy(void *__restrict dest, const void *__restrict src, std::size_t n) {
#if defined(__x86_64__)
    asm volatile(
        "rep movsb"
        : "+D"(dest), "+S"(src), "+c"(n)
        :
        : "memory"
    );
    return dest; 
#else
    std::uint8_t *__restrict pdest = static_cast<std::uint8_t *__restrict>(dest);
    const std::uint8_t *__restrict psrc = static_cast<const std::uint8_t *__restrict>(src);

    for (std::size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
    return dest;
#endif
}

void* klibc::memset(void *s, int c, std::size_t n) {
#if defined(__x86_64__)
    void* original_s = s;
    asm volatile(
        "rep stosb"
        : "+D"(s), "+c"(n)
        : "a"((std::uint8_t)c)
        : "memory"
    );
    return original_s;
#else
    std::uint8_t *p = static_cast<std::uint8_t *>(s);
    for (std::size_t i = 0; i < n; i++) {
        p[i] = static_cast<std::uint8_t>(c);
    }
    return s;
#endif
}
void* klibc::memmove(void *dest, const void *src, std::size_t n) {
    std::uint8_t *pdest = static_cast<std::uint8_t *>(dest);
    const std::uint8_t *psrc = static_cast<const std::uint8_t *>(src);

    if (reinterpret_cast<std::uintptr_t>(src) > reinterpret_cast<std::uintptr_t>(dest)) {
        for (std::size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (reinterpret_cast<std::uintptr_t>(src) < reinterpret_cast<std::uintptr_t>(dest)) {
        for (std::size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int klibc::memcmp(const void *s1, const void *s2, std::size_t n) {
    const std::uint8_t *p1 = static_cast<const std::uint8_t *>(s1);
    const std::uint8_t *p2 = static_cast<const std::uint8_t *>(s2);

    for (std::size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

int klibc::strlen(const char *str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}