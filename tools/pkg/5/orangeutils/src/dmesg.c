
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

uint64_t dmesg(char* buffer, uint64_t count) {
    int ret;
    uint64_t size;
    asm volatile("syscall" : "=a"(ret), "=d"(size) : "a"(64), "D"(buffer), "S"(count) : "rcx","r11");
    return size;
}

int main() {
    printf("Orange dmesg utility\n");
    uint64_t size = dmesg(0,0);
    char* buffer = malloc(size + 1);
    memset(buffer,0,size);
    dmesg(buffer,size);
    write(STDOUT_FILENO,buffer,size);
    return 0;
}