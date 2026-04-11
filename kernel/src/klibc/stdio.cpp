#include <klibc/stdio.hpp>
#include <klibc/string.hpp>
#include <utils/flanterm.hpp>
#include <cstdint>
#include <cstddef>
#include <cstdarg>

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_IMPLEMENTATION
#include <nanoprintf.h>

#include <cstdint>
#include <cstddef>
#if defined(__x86_64__)
#include <arch/x86_64/drivers/serial.hpp>
#endif
#include <generic/lock/spinlock.hpp>
#include <generic/userspace/syscall_list.hpp>

locks::spinlock print_lock;

int klibc::_snprintf(char *buffer, std::size_t bufsz, char const *fmt, va_list vlist) {
    int const rv = npf_vsnprintf(buffer, bufsz, fmt, vlist);
    return rv;
}

void klibc::printf(const char* fmt, ...) {
    print_lock.lock();
    va_list val;
    va_start(val, fmt);
    char buffer[4096];
    memset(buffer,0,4096);
    int len = _snprintf(buffer,4096,fmt,val);
    utils::flanterm::write(buffer,len);
#if defined(__x86_64__)
    x86_64::serial::write_data(buffer,len);
#endif
    va_end(val);
    print_lock.unlock();
}

void klibc::debug_printf(const char* fmt, ...) {
    print_lock.lock();
    va_list val;
    va_start(val, fmt);
    char buffer[4096];
    memset(buffer,0,4096);
    int len = _snprintf(buffer,4096,fmt,val);
#if defined(__x86_64__)
    (void)len;
    char buffer2[4096] = {};
    int len2 = klibc::__printfbuf(buffer2, 4096, "[pid %05d] %s", current_proc ? current_proc->id : -1, buffer);

    x86_64::serial::write_data(buffer2,len2);
#endif
    va_end(val);
    print_lock.unlock();
}