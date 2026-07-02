#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>

#define MAX_EVENTS 1
#define BUFFER_SIZE 64

int main() {
    int pipefd[2];
    int epollfd;
    struct epoll_event ev, events[MAX_EVENTS];
    char buf[BUFFER_SIZE];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = pipefd[0];
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefd[0], &ev) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    if (write(pipefd[1], "Hello, epoll!", 13) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
        perror("epoll_wait");
        exit(EXIT_FAILURE);
    }

    for (int n = 0; n < nfds; ++n) {
        if ((events[n].events & EPOLLIN) && (events[n].data.fd == pipefd[0])) {
            ssize_t nread = read(pipefd[0], buf, sizeof(buf) - 1);
            if (nread == -1) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            buf[nread] = '\0';
            printf("Received: %s\n", buf);
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);
    close(epollfd);

    return 0;
}
