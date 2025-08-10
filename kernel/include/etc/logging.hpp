
#pragma once

#define LEVEL_MESSAGE_OK 0
#define LEVEL_MESSAGE_FAIL 1
#define LEVEL_MESSAGE_WARN 2
#define LEVEL_MESSAGE_INFO 3

#include <flanterm.h>
#include <flanterm_backends/fb.h>

#include <cstdarg>

int __snprintf(char *buffer, size_t bufsz, char const *fmt, va_list vlist);
int __printfbuf(char* buffer, size_t bufsf, char const* fmt, ...);


class Log {
private:
    struct flanterm_context* ft_ctx0;
public:

    void Setup(struct flanterm_context* ft_ctx) {
        ft_ctx0 = ft_ctx;
    }

    void Write(char* buffer, int len) {
        flanterm_write(ft_ctx0,buffer,len);
    }

    static void Raw(char* msg,...);
    static void Init();
    static void Display(int level,char* msg,...);
    static void SerialDisplay(int level,char* msg,...);
};