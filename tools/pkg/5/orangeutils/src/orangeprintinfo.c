#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <pid> \n", argv[0]);
        return 1;
    }

    int pid = atoi(argv[1]);

    asm volatile("syscall" : : "a"(63), "D"(pid) : "rcx","r11");

    return 0;
}