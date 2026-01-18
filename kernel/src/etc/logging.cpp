
#include <etc/bootloaderinfo.hpp>
#include <etc/logging.hpp>

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_IMPLEMENTATION
#include <lib/nanoprintf.h>

#include <etc/libc.hpp>

#include <drivers/serial.hpp>

#include <generic/locks/spinlock.hpp>

#include <stddef.h>
#include <stdarg.h>

int __snprintf(char *buffer, size_t bufsz, char const *fmt, va_list vlist) {
    int const rv = npf_vsnprintf(buffer, bufsz, fmt, vlist);
    return rv;
}

Log LogObject;
uint32_t default_fg = 0xEEEEEEEE;
uint32_t default_fg_bright = 0xFFFFFFFF;

locks::spinlock log_lock;

void Log::Init() {
    struct limine_framebuffer* fb0 = BootloaderInfo::AccessFramebuffer();
    struct flanterm_context *ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        (uint32_t*)fb0->address, fb0->width, fb0->height, fb0->pitch,
        fb0->red_mask_size, fb0->red_mask_shift,
        fb0->green_mask_size, fb0->green_mask_shift,
        fb0->blue_mask_size, fb0->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, &default_fg,
        NULL, &default_fg_bright,
        NULL, 0, 0, 1,
        1, 1,
        0
    );
    LogObject.Setup(ft_ctx);
    
}

const char* level_messages[] = {
    [LEVEL_MESSAGE_OK] = "[   \x1b[38;2;0;255;0mOK\033[0m   ] ",
    [LEVEL_MESSAGE_FAIL] = "[ \x1b[38;2;255;0;0mFAILED\033[0m ] ",
    [LEVEL_MESSAGE_WARN] = "[  \x1b[38;2;255;165;0mWARN\033[0m  ] ",
    [LEVEL_MESSAGE_INFO] = "[  \x1b[38;2;0;191;255mINFO\033[0m  ] "
};

int __printfbuf(char* buffer, size_t bufsf, char const* fmt, ...) {
    va_list val;
    va_start(val, fmt);
    return __snprintf(buffer,bufsf,fmt,val);
    va_end(val);
}

void Log::SerialDisplay(int level,char* msg,...) {
    //log_lock.lock();
    va_list val;
    va_start(val, msg);
    char buffer[512];
    memset(buffer,0,512);

    drivers::serial serial(DEFAULT_SERIAL_PORT);

    serial.write((uint8_t*)level_messages[level],strlen(level_messages[level]));
    int len = __snprintf(buffer,512,msg,val);
    serial.write((uint8_t*)buffer,len);
    va_end(val);
    //log_lock.unlock();
}

void Log::Display(int level,char* msg,...) {
    va_list val;
    va_start(val, msg);
    char buffer[4096];
    memset(buffer,0,4096);
    log_lock.lock();
    LogObject.Write((char*)level_messages[level],strlen(level_messages[level]));
    int len = __snprintf(buffer,4096,msg,val);
    LogObject.Write(buffer,len);

    drivers::serial serial(DEFAULT_SERIAL_PORT);

    serial.write((uint8_t*)level_messages[level],strlen(level_messages[level]));
    serial.write((uint8_t*)buffer,len);

    log_lock.unlock();
    va_end(val);
}

void Log::Raw(char* msg,...) {
    va_list val;
    va_start(val, msg);
    char buffer[4096];
    memset(buffer,0,4096);
    log_lock.lock();
    int len = __snprintf(buffer,4096,msg,val);
    //LogObject.Write(buffer,len);

    drivers::serial serial(DEFAULT_SERIAL_PORT);

    serial.write((uint8_t*)buffer,len);

    log_lock.unlock();
    va_end(val);
}

#include <generic/mm/pmm.hpp>

char* dmesg_buf = 0;
std::uint64_t dmesg_size = 0;
std::uint64_t dmesg_real_size = 0;

std::uint64_t dmesg_bufsize() {
    return dmesg_size;
}

void dmesg_read(char* buffer,std::uint64_t count) {
    memset(buffer,0,count);
    memcpy(buffer,dmesg_buf,dmesg_size > count ? count : dmesg_size);
}

void dmesg0(char* msg,...) {

    if(!dmesg_buf) {
        dmesg_buf = (char*)memory::pmm::_virtual::alloc(4096);
        dmesg_real_size = 4096;
    }

    log_lock.lock();

    va_list val;
    va_start(val, msg);
    char buffer[4096];
    memset(buffer,0,4096);
    int len = __snprintf(buffer,4096,msg,val);

    drivers::serial serial(DEFAULT_SERIAL_PORT);
    serial.write((uint8_t*)buffer,len);

    uint64_t size = len;

    std::uint64_t offset = dmesg_size;
    std::uint64_t new_size = offset + size;

    if (new_size > dmesg_real_size) {
        alloc_t new_content0 = memory::pmm::_physical::alloc_ext(new_size);
        std::uint8_t* new_content = (std::uint8_t*)new_content0.virt;
        memcpy(new_content, dmesg_buf, dmesg_size); 
        memory::pmm::_physical::free((std::uint64_t)dmesg_buf);
        dmesg_buf = (char*)new_content;
        dmesg_real_size = new_content0.real_size;
    }

    dmesg_size = new_size;

    memcpy(dmesg_buf + offset, buffer, size);

    dmesg_size += size;

    log_lock.unlock();

    va_end(val);
}

#include <arch/x86_64/interrupts/idt.hpp>

extern void panic(int_frame_t* ctx, const char* msg);

void panic_wrap(const char* msg,...) {

    va_list val;
    va_start(val, msg);
    char buffer[4096];
    memset(buffer,0,4096);
    log_lock.lock();
    int len = __snprintf(buffer,4096,msg,val);

    panic(0,buffer);

    log_lock.unlock();
    va_end(val);
}