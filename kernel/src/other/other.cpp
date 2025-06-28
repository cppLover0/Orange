
#include <cstdint>
#include <cstddef>
#include <generic/memory/heap.hpp>
#include <other/other.hpp>
#include <other/string.hpp>

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

char* strrchr(const char *str, int c) {
    char* last = NULL;
    while (*str) {
        if (*str == c) {
            last = (char *)str;
        }
        str++;
    }

    if (*str == c) {
        last = (char *)str;
    }
    return last;
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
#include <arch/x86_64/interrupts/idt.hpp>
#include <arch/x86_64/cpu/data.hpp>
#include <generic/memory/pmm.hpp>

extern "C" uacpi_interrupt_ret handle_power_button(uacpi_handle ctx) {
    LogUnlock();
    Paging::EnableKernel();
    NLog("\n");
    Log(LOG_LEVEL_WARNING,"Shutdowning system in 5 seconds");

    __cli(); //yes i know lapic waiting for my eoi but why not ? 

    for(char i =0; i < 5;i++) {
        NLog(".");
        HPET::Sleep(1*1000*1000);
    }

    Log(LOG_LEVEL_INFO,"Bye.\n");

    uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);

    __hlt();
}

char* cwd_get(const char *path) {

    char* result = (char*)PMM::VirtualAlloc();
    memcpy(result,path,String::strlen((char*)path));

    char *last_slash = strrchr(result, '/');
    
    if (last_slash != NULL) {
        if (last_slash != result) {
            *last_slash = '\0';
        } else {
            *(last_slash + 1) = '\0';
        }
    } else {
        result[0] = '\0';
    }

    return result;

}
char* join_paths(const char* path1, const char* path2) {
    char* result = (char*)PMM::VirtualAlloc();
    if (!result) {
        return 0;
    }

    if (path2[0] == '/') {
        int i = 0;
        while (path2[i] != '\0' && i < 4095) {
            result[i] = path2[i];
            i++;
        }
        result[i] = '\0';
        return result;
    }

    int len1 = 0;
    while (path1[len1] != '\0') {
        len1++;
    }

    if (len1 > 0 && path1[len1-1] == '/') {
        len1--;
    }

    if (len1 == 1 && path1[0] == '/') {
        int i = 0;
        result[0] = '/';
        i = 1;
        int j = 0;
        while (path2[j] != '\0' && i < 4095) {
            result[i] = path2[j];
            i++;
            j++;
        }
        result[i] = '\0';
        return result;
    }

    int offset2 = 0;
    if (path2[0] == '.' && path2[1] == '/') {
        path2 += 2; 
        offset2 = 2;
    } else if (path2[0] == '.' && path2[1] == '.' && path2[2] == '/') {
        while (len1 > 0 && path1[len1-1] != '/') {
            len1--;
        }
        if (len1 > 0) {
            len1--;
        }
        path2 += 3; 
        offset2 = 3;
    }

    int i = 0;
    for (int j = 0; j < len1 && i < 4095; j++) {
        result[i] = path1[j];
        i++;
    }

    if (i < 4095) {
        result[i] = '/';
        i++;
    }

    int j = offset2;
    while (path2[j-offset2] != '\0' && i < 4095) {
        result[i] = path2[j-offset2];
        i++;
        j++;
    }

    result[i] = '\0';

    return result;
}