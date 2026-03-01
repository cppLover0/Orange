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

int klibc::_snprintf(char *buffer, std::size_t bufsz, char const *fmt, va_list vlist) {
    int const rv = npf_vsnprintf(buffer, bufsz, fmt, vlist);
    return rv;
}

void klibc::printf(const char* fmt, ...) {
    va_list val;
    va_start(val, fmt);
    char buffer[4096];
    memset(buffer,0,4096);
    int len = _snprintf(buffer,4096,fmt,val);
    utils::flanterm::write(buffer,len);
    va_end(val);
}