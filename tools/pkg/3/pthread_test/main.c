#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <termios.h>

// Function to be executed by the thread
void* print_message(void* thread_id) {
    long tid = (long)thread_id;
    printf("Hello from thread %ld\n", tid);
    pthread_exit(NULL);
}

int main() {
    struct pollfd fd;
    fd.events = POLLIN;
    fd.fd = STDIN_FILENO;
    struct termios t;
    tcgetattr(STDIN_FILENO,&t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&t);
    while(1) {
        if(poll(&fd,1,100) > 0) {
            if(fd.revents & POLLIN) {
                printf("got key %c\n",getchar());
            }
        }
    }
    return 0;
}
