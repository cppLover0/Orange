#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <poll.h>

#define SOCKET_PATH "/tmp/test_unix_socket"
#define THREADS 10

void* client_thread(void* arg) {
    (void)arg;
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("client socket");
        return NULL;
    }

    struct sockaddr_un serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    while (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        usleep(100000);
    }

    int one = 1;
    while (1) {
        ssize_t n = write(sock, &one, sizeof(one));
        if (n != sizeof(one)) {
            perror("client write");
            break;
        }

    }

    close(sock);
    return NULL;
}

int main() {

    unlink(SOCKET_PATH);

    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listen_fd, THREADS) < 0) {
        perror("listen");
        exit(1);
    }

    pthread_t clients[THREADS];
    for (int i = 0; i < THREADS; i++) {
        if (pthread_create(&clients[i], NULL, client_thread, NULL) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    int client_fds[THREADS];
    for (int i = 0; i < THREADS; i++) {
        client_fds[i] = accept(listen_fd, NULL, NULL);
        if (client_fds[i] < 0) {
            perror("accept");
            exit(1);
        }
        printf("Accepted client %d\n", i);
    }

    struct pollfd pfds[THREADS];
    for (int i = 0; i < THREADS; i++) {
        pfds[i].fd = client_fds[i];
        pfds[i].events = POLLIN;
    }

    while (1) {
        int ret = poll(pfds, THREADS, -1);
        if (ret < 0) {
            perror("poll");
            break;
        }

        for (int i = 0; i < THREADS; i++) {
            if (pfds[i].revents & POLLIN) {
                int val;
                ssize_t n = read(pfds[i].fd, &val, sizeof(val));
                if (n == 0) {
                    printf("Client %d disconnected\n", i);
                    close(pfds[i].fd);
                    pfds[i].fd = -1;
                } else if (n == sizeof(val)) {
                    if (val != 1) {
                        printf("bug got %d from client %d\n", val, i);
                    } else {
                        printf("good value from client %d\n",i);
                    }
                } else {
                    perror("read");
                }
            }
        }
    }

    for (int i = 0; i < THREADS; i++) {
        if (pfds[i].fd != -1)
            close(pfds[i].fd);
    }
    close(listen_fd);
    unlink(SOCKET_PATH);

    return 0;
}
