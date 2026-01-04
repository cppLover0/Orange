#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

void handle_sigusr1(int sig) {
    printf("got SIGUSR1!\n");
    void *ret_addr = __builtin_return_address(0);
    printf("ret: %p\n", ret_addr);
}

void handle_sigusr2(int sig) {
    printf("got SIGUSR2!\n");
    void *ret_addr = __builtin_return_address(0);
    printf("ret: %p\n", ret_addr);
}

void test() {
    void *ret_addr = __builtin_return_address(0);
    printf("ret: %p\n", ret_addr);
}

int main() {
    pid_t pid;

    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    test();
    printf("%p\n",main);


    pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {

        printf("child start. pid = %d\n", getpid());
        while (1) {
            printf("wait signal...\n");
            pause();
        }
    } else {

        printf("parent start. pid = %d, child pid = %d\n", getpid(), pid);
        sleep(2);


        printf("parent send SIGUSR1\n");
        kill(pid, SIGUSR1);

        sleep(2);

        printf("parent send SIGUSR2\n");
        kill(pid, SIGUSR2);

        sleep(2);
        kill(pid, SIGTERM);  
        printf("parent end.\n");
    }

    return 0;
}