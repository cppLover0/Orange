#include <cstdint>
#include <klibc/string.hpp>

extern "C" {

void *memcpy(void *__restrict dest, const void *__restrict src, std::size_t n) {
    return klibc::memcpy(dest, src, n);
}

void *memset(void *s, int c, std::size_t n) {
    return klibc::memset(s, c, n);
}

void *memmove(void *dest, const void *src, std::size_t n) {
    return klibc::memmove(dest, src, n);
}

int memcmp(const void *s1, const void *s2, std::size_t n) {
    return klibc::memcmp(s1, s2, n);
}

}

#include <generic/arch.hpp>

extern "C" {
    int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
    void __cxa_pure_virtual() { arch::hcf(); }
    void *__dso_handle;
}

#include <generic/heap.hpp>

typedef unsigned long size_t;

void *operator new(size_t size)
{
    void* p = kheap::malloc(size);
    memset(p,0,size);
    return p;
}

void *operator new[](size_t size)
{
    void* p = kheap::malloc(size);
    memset(p,0,size);
    return p;
}

void operator delete(void *p)
{
    kheap::free(p);
}

void operator delete[](void *p)
{
    kheap::free(p);
}

void operator delete(void *p, size_t size)
{
    (void)size;
    kheap::free(p);
}

void operator delete[](void *p, size_t size)
{
    (void)size;
    kheap::free(p);
}