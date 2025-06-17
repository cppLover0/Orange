
#include <stdio.h>

int main() {
    asm volatile("syscall" : : "a"(41) : "rcx","r11");
}