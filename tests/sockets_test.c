#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <sys/wait.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/unix_socket_example"
#define BUF_SIZE 128

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void run_server(const char *mode) {
    int listen_fd, conn_fd;
    struct sockaddr_un addr;
    char buffer[BUF_SIZE];
    struct pollfd fds;

    if ((listen_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) die("socket server");
    unlink(SOCKET_PATH);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) die("bind");
    if (listen(listen_fd, 5) == -1) die("listen");

    if ((conn_fd = accept(listen_fd, NULL, NULL)) == -1) die("accept");

    fds.fd = conn_fd;
    fds.events = POLLIN;

    if (poll(&fds, 1, 5000) > 0 && (fds.revents & POLLIN)) {
        ssize_t n = 0;
        if (strcmp(mode, "read") == 0) {
            n = read(conn_fd, buffer, sizeof(buffer) - 1);
        } else if (strcmp(mode, "recv") == 0) {
            n = recv(conn_fd, buffer, sizeof(buffer) - 1, 0);
        } else if (strcmp(mode, "recvfrom") == 0) {
            n = recvfrom(conn_fd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
        }

        if (n <= 0) die("receive error");
        buffer[n] = '\0';
        printf("Server (%s): %s\n", mode, buffer);
    }

    close(conn_fd);
    close(listen_fd);
    unlink(SOCKET_PATH);
}

void run_client(const char *mode) {
    int sock_fd;
    struct sockaddr_un addr;
    const char *msg = "Hello";

    sleep(1);
    if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) die("socket client");
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) die("connect");

    if (strcmp(mode, "write") == 0) {
        if (write(sock_fd, msg, strlen(msg)) == -1) die("write");
    } else if (strcmp(mode, "send") == 0) {
        if (send(sock_fd, msg, strlen(msg), 0) == -1) die("send");
    } else if (strcmp(mode, "sendto") == 0) {
        if (sendto(sock_fd, msg, strlen(msg), 0, NULL, 0) == -1) die("sendto");
    }

    close(sock_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <send_mode> <recv_mode>\n", argv[0]);
        fprintf(stderr, "Send: write|send|sendto\nRecv: read|recv|recvfrom\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) die("fork");

    if (pid == 0) {
        run_server(argv[2]);
        exit(0);
    } else {
        run_client(argv[1]);
        wait(NULL);
    }

    return 0;
}
