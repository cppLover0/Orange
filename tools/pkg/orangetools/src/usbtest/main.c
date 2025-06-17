
#include <stdio.h>

int main() {
    printf("UsbTest Utility\n");
    asm volatile("syscall" : : "a"(39) : "rcx","r11");
}