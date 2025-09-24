#include <stdio.h>
#include <poll.h>
#include <unistd.h>

#include <termios.h>

#include <string.h>

int main() {
    struct pollfd fds[1];
    fds[0].fd = 0; 
    fds[0].events = POLLIN; 

    struct termios newt;
    tcgetattr(STDIN_FILENO, &newt);
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); 

    printf("Waiting for input (5 second timeout)...\n");
    int ret = poll(fds, 1, 5000); 

    if (ret == -1) {
        perror("poll");
        return 1;
    } else if (ret == 0) {
        printf("Timeout !\n");
    } else {
        if (fds[0].revents & POLLIN) {
            printf("Input ! waiting for string\n");
            char buf[100];
            read(STDIN_FILENO,buf,100);
            memset(buf,0,100);
            newt.c_lflag |= ICANON | ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &newt); 
            read(STDIN_FILENO,buf,100);
            printf("You enter: %s", buf);
        }
    }

    newt.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); 

    return 0;
}