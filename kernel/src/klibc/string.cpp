
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

char* klibc::strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return nullptr;
}

char* klibc::strtok(char **next,char *str, const char *delim) {
    if (str) *next = str;
    if (!*next) return nullptr;

    char *start = *next;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    if (!*start) {
        *next = nullptr;
        return nullptr;
    }

    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }

    if (*end) {
        *end = '\0';
        *next = end + 1;
    } else {
        *next = nullptr;
    }

    return start;
}

char* klibc::strrchr(const char* str, int ch) {
    char* last_occurrence = 0;

    while (*str) {
        if (*str == ch) {
            last_occurrence = (char*)str; 
        }
        str++;
    }

    return last_occurrence;
}

char* klibc::strcat(char* dest, const char* src) {

    char* ptr = dest;
    while(*ptr != '\0') {
        ptr++;
    }
    
    while(*src != '\0') {
        *ptr = *src;
        ptr++;
        src++;
    }
    
    *ptr = '\0';
    
    return dest;
}

int klibc::strncmp(const char *s1, const char *s2, std::size_t n) {
    std::size_t i = 0;
    while (i < n && s1[i] && (s1[i] == s2[i])) {
        i++;
    }
    if (i == n) return 0;
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

// tbh i dont remember from what i took this
int klibc::strcmp(const char *s1, const char *s2) {
    const unsigned long *w1 = (const unsigned long *)s1;
    const unsigned long *w2 = (const unsigned long *)s2;
    const unsigned long himagic = 0x8080808080808080UL;
    const unsigned long lomagic = 0x0101010101010101UL;
    
    while (1) {
        unsigned long word1 = *w1++;
        unsigned long word2 = *w2++;
        
        unsigned long diff = word1 ^ word2;
        unsigned long zeros = (word1 - lomagic) & ~word1 & himagic;
        
        if (diff != 0 || zeros != 0) {
            const char *c1 = (const char *)(w1 - 1);
            const char *c2 = (const char *)(w2 - 1);
            
            do {
                unsigned char ch1 = *c1++;
                unsigned char ch2 = *c2++;
                if (ch1 != ch2) return ch1 - ch2;
                if (ch1 == 0) break;
            } while (1);
            
            return 0;
        }
    }
}