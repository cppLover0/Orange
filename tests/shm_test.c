#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_SIZE 4096
#define TEST_MSG "Hello from System V SHM!"

int main() {
    int shmid = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget error");
        return 1;
    }

    void *ptr = shmat(shmid, NULL, 0);
    if (ptr == (void *)-1) {
        perror("shmat error");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork error");
        shmdt(ptr);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    } 
    else if (pid == 0) {
        sleep(1);
        printf("[Child] Reading: %s\n", (char *)ptr);
        shmdt(ptr);
        exit(0);
    } 
    else {
        printf("[Parent] Writing...\n");
        memcpy(ptr, TEST_MSG, strlen(TEST_MSG) + 1);
        wait(NULL);
        shmdt(ptr);
        shmctl(shmid, IPC_RMID, NULL);
        printf("[Parent] Done.\n");
    }

    return 0;
}
