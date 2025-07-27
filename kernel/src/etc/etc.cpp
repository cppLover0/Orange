
#include <etc/etc.hpp>
#include <cstdint>
#include <cstddef>

extern "C" {

void *memcpy(void *__restrict dest, const void *__restrict src, std::size_t n) {
    std::uint8_t *__restrict pdest = static_cast<std::uint8_t *__restrict>(dest);
    const std::uint8_t *__restrict psrc = static_cast<const std::uint8_t *__restrict>(src);

    for (std::size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, std::size_t n) {
    std::uint8_t *p = static_cast<std::uint8_t *>(s);

    for (std::size_t i = 0; i < n; i++) {
        p[i] = static_cast<uint8_t>(c);
    }

    return s;
}

void *memmove(void *dest, const void *src, std::size_t n) {
    std::uint8_t *pdest = static_cast<std::uint8_t *>(dest);
    const std::uint8_t *psrc = static_cast<const std::uint8_t *>(src);

    if (src > dest) {
        for (std::size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (std::size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, std::size_t n) {
    const std::uint8_t *p1 = static_cast<const std::uint8_t *>(s1);
    const std::uint8_t *p2 = static_cast<const std::uint8_t *>(s2);

    for (std::size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

}

extern void (*__init_array[])();
extern void (*__init_array_end[])();

void Other::ConstructorsInit() {
    for (std::size_t i = 0; &__init_array[i] != __init_array_end; i++) {
        __init_array[i]();
    }
}

extern "C" {
    int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
    void __cxa_pure_virtual() { asm volatile("hlt"); }
    void *__dso_handle;
}

#include <generic/mm/heap.hpp>


void *operator new(size_t size)
{
    return memory::heap::malloc(size);
}

void *operator new[](size_t size)
{
    return memory::heap::malloc(size);
}

void operator delete(void *p)
{
    memory::heap::free(p);
}

void operator delete(void *p,unsigned long sz)
{
    memory::heap::free(p);
}

void operator delete[](void *p)
{
    memory::heap::free(p);
}
