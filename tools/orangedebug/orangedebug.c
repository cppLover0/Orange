#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <program> [args...]\n", argv[0]);
        return 1;
    }

#if defined(__x86_64__)
    asm volatile("syscall" : : "a"(84) : "rcx","r11");
#endif

    execvp(argv[1], &argv[1]);
    perror("execvp");
    return 1;
}