
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

void dmesg0(char* msg,...);
std::uint64_t dmesg_bufsize();

void dmesg_read(char* buffer,std::uint64_t count);

#include <drivers/tsc.hpp>

void panic_wrap(const char* msg, ...);

#define assert(cond, msg,...) if(!(cond)) panic_wrap("Failed assert at %s:%s:%d \"" msg "\"\n" , __FILE__ ,__FUNCTION__, __LINE__ , ##__VA_ARGS__)

#define dmesg(fmt,...) if(1) dmesg0("[%llu] %s: " fmt "\n", drivers::tsc::currentus() ,__FUNCTION__, ##__VA_ARGS__)
#define BREAKPOINT() Log::Display(LEVEL_MESSAGE_INFO,"breakpoint %s:%d\n ",__FILE__,__LINE__)

#define DEBUG(is_enabled,fmt,...) if(is_enabled) Log::SerialDisplay(LEVEL_MESSAGE_INFO,"[%llu] %s: " fmt "\n", drivers::tsc::currentus() ,__FUNCTION__, ##__VA_ARGS__)
#define STUB(is_enabled) if(is_enabled) Log::SerialDisplay(LEVEL_MESSAGE_INFO, "%s() is a stub !\n", __FUNCTION__)