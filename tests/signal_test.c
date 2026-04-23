#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

void handle_sig(int sig) {
    const char *msg = "Caught signal!\n";
    write(STDOUT_FILENO, msg, 15); 
    exit(0);
}

void* killer_thread(void* arg) {
    sleep(2);
    kill(getpid(), SIGUSR1);
    return NULL;
}

int main() {
    pthread_t thread;

    signal(SIGUSR1, handle_sig);

    printf("Main process PID: %d\n", getpid());

    if (pthread_create(&thread, NULL, killer_thread, NULL) != 0) {
        return 1;
    }

    while (1) {
        printf("Waiting for thread to kill me...\n");
        sleep(10);
    }

    return 0;
}
