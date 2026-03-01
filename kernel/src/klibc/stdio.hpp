#pragma once

#include <cstdint>
#include <cstdarg>

namespace klibc {

    int _snprintf(char *buffer, std::size_t bufsz, char const *fmt, va_list vlist);

    inline static int __printfbuf(char* buffer, std::size_t bufsf, char const* fmt, ...) {
        va_list val;
        va_start(val, fmt);
        return _snprintf(buffer,bufsf,fmt,val);
        va_end(val);
    }

    void printf(const char* fmt, ...);
};