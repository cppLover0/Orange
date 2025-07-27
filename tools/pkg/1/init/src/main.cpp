
#include <stdio.h>

int main() {
    const char* msg = "Hello, World from libc !\n";
    asm volatile("syscall" : : "a"(9), "D"(msg) : "rcx","r11");
}