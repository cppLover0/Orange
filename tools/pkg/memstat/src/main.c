
#include <stdio.h>

int main() {
    printf("MemStat Utility\n");
    asm volatile("syscall" : : "a"(38) : "rcx","r11");
}