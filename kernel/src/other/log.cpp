
#include <generic/locks/spinlock.hpp>
#include <drivers/serial/serial.hpp>
#include <generic/status/status.hpp>
#include <other/string.hpp>
#include <stdint.h>
#include <stdarg.h>
#include <other/log.hpp>
#include <lib/flanterm/flanterm.h>
#include <drivers/cmos/cmos.hpp>
#include <lib/flanterm/backends/fb.h>

char log_lock = 0;
flanterm_context* ft_ctx;

void LogInit(char* ctx) {
    ft_ctx = (flanterm_context*)ctx;
}

void LogUnlock() {
    log_lock = 0;
}

void LogBuffer(char* buffer,uint64_t size) {
    for(uint64_t i = 0;i < size;i++) {
        flanterm_write(ft_ctx,&buffer[i],1);
        Serial::Write(buffer[i]);
    }
}

void NLog(char* format, ...) {
    spinlock_lock(&log_lock);
    va_list args;
    va_start(args, format);
    int i = 0;

    while (i < String::strlen(format)) {
        if (format[i] == '%') {
            i++;
            char buffer[256];
            if (format[i] == 'd' || format[i] == 'i') {
                String::itoa(va_arg(args, uint64_t), buffer,10);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 's') {
                char* buffer1 = va_arg(args, char*);
                Serial::WriteString(buffer1);
                flanterm_write(ft_ctx,buffer1,String::strlen(buffer1));
            } else if (format[i] == 'p') {
                String::itoa(va_arg(args,uint64_t),buffer,16);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 'c') {
                buffer[0] = (char)va_arg(args,uint64_t);
                buffer[1] = '\0';
                Serial::WriteString(buffer);
                flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            }
        } else {
            Serial::Write(format[i]);
            flanterm_write(ft_ctx,&format[i],1);
        }
        i++;
    }
    va_end(args);
    spinlock_unlock(&log_lock);
}

void LogSerial(char* format, ...) { // actually serial doesnt need spinlock
    va_list args;
    va_start(args, format);
    int i = 0;

    char sec_b[4];
    char min_b[4];
    char hour_b[4];

    String::itoa(CMOS::Second(), sec_b,10);
    String::itoa(CMOS::Minute(), min_b,10);
    String::itoa(CMOS::Hour(), hour_b,10);

    Serial::WriteString("[");

    Serial::WriteString(hour_b);
    Serial::WriteString(".");

    Serial::WriteString(min_b);

    Serial::WriteString(".");

    Serial::WriteString(sec_b);

    Serial::WriteString("] ");

    while (i < String::strlen(format)) {
        if (format[i] == '%') {
            i++;
            char buffer[256];
            if (format[i] == 'd' || format[i] == 'i') {
                String::itoa(va_arg(args, uint64_t), buffer,10);
                buffer[255] = 0;
                Serial::WriteString(buffer);
            } else if (format[i] == 's') {
                char* buffer1 = va_arg(args, char*);
                Serial::WriteString(buffer1);
            } else if (format[i] == 'p') {
                String::itoa(va_arg(args,uint64_t),buffer,16);
                buffer[255] = 0;
                Serial::WriteString(buffer);
            } else if (format[i] == 'c') {
                buffer[0] = (char)va_arg(args,uint64_t);
                buffer[1] = '\0';
                Serial::WriteString(buffer);
            }
        } else {
            Serial::Write(format[i]);
        }
        i++;
    }
    va_end(args);
}

const char* level_messages[] = {
    [LOG_LEVEL_INFO] =  "[ \x1b[38;2;0;127;255mINFO\x1b[38;2;255;255;255m  ] ",
    [LOG_LEVEL_ERROR] = "[ \x1b[38;2;255;0;0mFAULT\x1b[38;2;255;255;255m ] ",
    [LOG_LEVEL_WARNING] = "[ \x1b[38;2;255;165;0mWARN\x1b[38;2;255;255;255m  ] ",
    [LOG_LEVEL_DEBUG] = "[ \x1b[38;2;150;150;150mDEBUG\x1b[38;2;255;255;255m ] "
};

void DLog(char* format, ...) {
    spinlock_lock(&log_lock);
    va_list args;
    va_start(args, format);
    int i = 0;

    int level = LOG_LEVEL_DEBUG;

    //flanterm_write(ft_ctx,level_messages[level],String::strlen((char*)level_messages[level]));
    Serial::WriteString(level_messages[level]);

    while (i < String::strlen(format)) {
        if (format[i] == '%') {
            i++;
            char buffer[256];
            if (format[i] == 'd' || format[i] == 'i') {
                String::itoa(va_arg(args, uint64_t), buffer,10);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                //flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 's') {
                char* buffer1 = va_arg(args, char*);
                Serial::WriteString(buffer1);
                //flanterm_write(ft_ctx,buffer1,String::strlen(buffer1));
            } else if (format[i] == 'p') {
                String::itoa(va_arg(args,uint64_t),buffer,16);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                //flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 'c') {
                buffer[0] = (char)va_arg(args,uint64_t);
                buffer[1] = '\0';
                Serial::WriteString(buffer);
                //flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            }
        } else {
            Serial::Write(format[i]);
            //flanterm_write(ft_ctx,&format[i],1);
        }
        i++;
    }
    va_end(args);
    spinlock_unlock(&log_lock);
}

void SLog(int level,char* format, ...) {
    spinlock_lock(&log_lock);
    va_list args;
    va_start(args, format);
    int i = 0;

    if(level == LOG_LEVEL_DEBUG) {
        asm volatile("nop");
    }

    //flanterm_write(ft_ctx,level_messages[level],String::strlen((char*)level_messages[level]));
    Serial::WriteString(level_messages[level]);

    while (i < String::strlen(format)) {
        if (format[i] == '%') {
            i++;
            char buffer[256];
            if (format[i] == 'd' || format[i] == 'i') {
                String::itoa(va_arg(args, uint64_t), buffer,10);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                //flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 's') {
                char* buffer1 = va_arg(args, char*);
                Serial::WriteString(buffer1);
                //flanterm_write(ft_ctx,buffer1,String::strlen(buffer1));
            } else if (format[i] == 'p') {
                String::itoa(va_arg(args,uint64_t),buffer,16);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                //flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 'c') {
                buffer[0] = (char)va_arg(args,uint64_t);
                buffer[1] = '\0';
                Serial::WriteString(buffer);
                //flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            }
        } else {
            Serial::Write(format[i]);
            //flanterm_write(ft_ctx,&format[i],1);
        }
        i++;
    }
    va_end(args);
    spinlock_unlock(&log_lock);
}

void Log(int level,char* format, ...) {
    spinlock_lock(&log_lock);
    va_list args;
    va_start(args, format);
    int i = 0;

    if(level == LOG_LEVEL_DEBUG) {
#ifdef NO_LEVEL_DEBUG
        return;
#endif
        asm volatile("nop");
    }

    flanterm_write(ft_ctx,level_messages[level],String::strlen((char*)level_messages[level]));
    Serial::WriteString(level_messages[level]);

    while (i < String::strlen(format)) {
        if (format[i] == '%') {
            i++;
            char buffer[256];
            if (format[i] == 'd' || format[i] == 'i') {
                String::itoa(va_arg(args, uint64_t), buffer,10);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 's') {
                char* buffer1 = va_arg(args, char*);
                Serial::WriteString(buffer1);
                flanterm_write(ft_ctx,buffer1,String::strlen(buffer1));
            } else if (format[i] == 'p') {
                String::itoa(va_arg(args,uint64_t),buffer,16);
                buffer[255] = 0;
                Serial::WriteString(buffer);
                flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            } else if (format[i] == 'c') {
                buffer[0] = (char)va_arg(args,uint64_t);
                buffer[1] = '\0';
                Serial::WriteString(buffer);
                flanterm_write(ft_ctx,buffer,String::strlen(buffer));
            }
        } else {
            Serial::Write(format[i]);
            flanterm_write(ft_ctx,&format[i],1);
        }
        i++;
    }
    va_end(args);
    spinlock_unlock(&log_lock);
}