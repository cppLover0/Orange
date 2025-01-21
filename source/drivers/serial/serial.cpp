
#include <generic/locks/spinlock.hpp>
#include <generic/status/status.hpp>
#include <other/string.hpp>
#include <stdint.h>
#include <stdarg.h>

char serial_spinlock;

orange_status Serial::Init() {
    spinlock_lock(&serial_spinlock);
    orange_status status = 0;
    IO::OUT(COM1 + 1,0,1);
    IO::OUT(COM1 + 3,(1 << 7),1);
    IO::OUT(COM1 + 1,0,1);
    IO::OUT(COM1,12,1);
    IO::OUT(COM1 + 3,0,1);
    IO::OUT(COM1 + 3,(1 << 1) | (1 << 0),1);
    IO::OUT(COM1 + 4,(1 << 0) | (1 << 2) | (1 << 3),1);
    IO::OUT(COM1 + 4,(1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),1);
    IO::OUT(COM1,0xAE,1);
    if(IO::IN(COM1,1) != 0xAE) {
        status |= ORANGE_STATUS_SOMETHING_WRONG_STATE | ORANGE_STATUS_NOT_SUPPORTED;
        spinlock_unlock(&serial_spinlock);
        return status;
    }
    IO::OUT(COM1 + 4,(1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),1);
    status |= ORANGE_STATUS_IS_ALL_OK | ORANGE_STATUS_INITIALIZIED_STATE | (1 << 4);
    spinlock_unlock(&serial_spinlock);
    return status;
}

uint8_t Serial::Read() {
    spinlock_lock(&serial_spinlock);
    while(IO::IN(COM1 + 5,1) & 1 == 0) {} 
    uint8_t value = IO::IN(COM1,1);
    spinlock_unlock(&serial_spinlock);
    return value;
}

void Serial::Write(uint8_t data) {
    spinlock_lock(&serial_spinlock);
    while(IO::IN(COM1 + 5,1) & (1 << 5) == 0) {}
    IO::OUT(COM1,data,1);
    spinlock_unlock(&serial_spinlock);
}

void Serial::WriteArray(uint8_t* data,uint64_t len) {
    for(uint64_t i = 0; i < len;i++) {
        Write(data[i]);
    }
}

void Serial::WriteString(const char* str) {
    Serial::WriteArray((uint8_t*)str,String::strlen((char*)str));
}

void Serial::printf(char* format, ...) {
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
                WriteString(buffer);
            } else if (format[i] == 's') {
                WriteString(va_arg(args, char*));
            } else if (format[i] == 'p') {
                String::itoa(va_arg(args,uint64_t),buffer,16);
                buffer[255] = 0;
                WriteString(buffer);
            } 
        } else {
            Write(format[i]);
        }
        i++;
    }
    va_end(args);
}
