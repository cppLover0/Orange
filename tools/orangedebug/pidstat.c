#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s pid\n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);
#if defined(__x86_64__)
    int ret = 0;
    asm volatile("syscall" : "=a"(ret) : "a"(100), "D"(pid) : "rcx", "r11");
#endif

    return 1;
}