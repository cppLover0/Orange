
#include <cstdint>
#include <cstddef>
#include <generic/memory/heap.hpp>

extern "C" {

void *memcpy(void *dest, const void *src, std::size_t n) {
    std::uint8_t *pdest = static_cast<std::uint8_t *>(dest);
    const std::uint8_t *psrc = static_cast<const std::uint8_t *>(src);

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

namespace {

void hcf() {
    for (;;) {
        asm ("hlt");
    }
}

}

extern "C" {
    int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
    void __cxa_pure_virtual() { hcf(); }
    void *__dso_handle;
}

void *operator new(size_t size)
{
    return KHeap::Malloc(size);
}

void *operator new[](size_t size)
{
    return KHeap::Malloc(size);
}

void operator delete(void *p)
{
    KHeap::Free(p);
}

void operator delete[](void *p)
{
    KHeap::Free(p);
}

#include <uacpi/sleep.h>
#include <uacpi/event.h>
#include <other/log.hpp>
#include <generic/memory/paging.hpp>
#include <other/assembly.hpp>
#include <drivers/hpet/hpet.hpp>
#include <drivers/ps2keyboard/ps2keyboard.hpp>

extern "C" void keyHandler() {
    __cli();
    LogUnlock();
    Paging::EnableKernel();
    char key = PS2Keyboard::Get();
    NLog("%c",key);
    PS2Keyboard::EOI();
}

extern "C" uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    LogUnlock();
    Paging::EnableKernel();
    NLog("\n");
    Log("Shutdowning system in 5 seconds");

    __cli(); //yes i know lapic waiting for my eoi but why not ? 

    for(char i =0; i < 5;i++) {
        NLog(".");
        HPET::Sleep(1*1000*1000);
    }

    Log("\nBye.\n");

    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);

    __hlt();
}
