
#include <stdio.h>
#include <stdint.h>

int main() {
    printf("MemStat Utility\n");
    uint64_t total_mem = 0;
    uint64_t free_mem = 0;
    asm volatile("syscall" : "=b"(total_mem), "=d"(free_mem) : "a"(46) : "rcx","r11");
    printf("Memory %d/%d KB : %d/%d MB\n",(total_mem - free_mem) / 1024,total_mem / 1024,((total_mem - free_mem) / 1024) / 1024,(total_mem / 1024) / 1024);
}