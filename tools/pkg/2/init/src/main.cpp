
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

void debug_log(void* msg) {
    asm volatile("syscall" : : "a"(9), "D"(msg) : "rcx","r11");
}

int main() {
    int pid = fork();
    if(pid == 0) {
        debug_log((void*)"Hello from child process !");
    } else {
        debug_log((void*)"Hello from parent process !");
    }
}