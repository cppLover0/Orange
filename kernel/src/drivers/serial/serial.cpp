
#include <generic/locks/spinlock.hpp>
#include <drivers/serial/serial.hpp>
#include <generic/status/status.hpp>
#include <other/string.hpp>
#include <stdint.h>
#include <stdarg.h>
#include <generic/tty/tty.hpp>

char serial_spinlock = 0;

void Serial::Init() {
    spinlock_lock(&serial_spinlock);
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
        spinlock_unlock(&serial_spinlock);
        return;
    }
    IO::OUT(COM1 + 4,(1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),1);
    spinlock_unlock(&serial_spinlock);
}

void __serial_process_fetch() {

    IO::IN(COM1,1);
    while(1) {
        uint8_t key = Serial::Read();
        __tty_receive_ipc(key);
    }
}

uint8_t Serial::Read() {
    //spinlock_lock(&serial_spinlock);
    while(!(IO::IN(COM1 + 5,1) & 1)) {} 
    uint8_t value = IO::IN(COM1,1);
    //spinlock_unlock(&serial_spinlock);
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
