#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#define SHM_SIZE 1024  // size of shared memory segment

int main() {
    key_t key;
    int shmid;
    char *shmaddr;

    // Generate a unique key for shared memory
    key = 100;

    // Create shared memory segment with shmget()
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0644);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    printf("Shared memory ID: %d\n", shmid);

    // Attach to the shared memory segment with shmat()
    shmaddr = (char*) shmat(shmid, NULL, 0);
    if (shmaddr == (char*) -1) {
        perror("shmat");
        exit(1);
    }

    // Write to shared memory
    strcpy(shmaddr, "Hello from shared memory!");

    printf("Data written to shared memory: %s\n", shmaddr);

    // Detach from shared memory with shmdt()
    if (shmdt(shmaddr) == -1) {
        perror("shmdt");
        exit(1);
    }

    // Note: To remove the shared memory segment, use shmctl(shmid, IPC_RMID, NULL);

    return 0;
}