
#include <stdio.h>

int main() {
    asm volatile("syscall" : : "a"(40) : "rcx","r11");
}