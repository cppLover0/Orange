#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

#define SOCKET_PATH "/tmp/my_unix_socket"
#define BUFFER_SIZE 128

void* server_thread(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_un addr;

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        pthread_exit(NULL);
    }

    unlink(SOCKET_PATH);
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        close(server_fd);
        pthread_exit(NULL);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        pthread_exit(NULL);
    }

    printf("server start\n");

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        pthread_exit(NULL);
    }
    printf("client connect\n");

    struct msghdr msg = {0};
    struct iovec iov;
    char buf[BUFFER_SIZE];

    char cmsg_buf[CMSG_SPACE(sizeof(struct ucred))];

    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    ssize_t num_bytes = recvmsg(client_fd, &msg, 0);
    if (num_bytes == -1) {
        perror("recvmsg");
        close(client_fd);
        close(server_fd);
        unlink(SOCKET_PATH);
        pthread_exit(NULL);
    }

    printf("got msg: %.*s\n", (int)num_bytes, buf);

    struct ucred *cred = NULL;
    struct cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg,cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
            cred = (struct ucred *)CMSG_DATA(cmsg);
            break;
        }
    }
    if (cred != NULL) {
        printf("uid: %d\n", cred->uid);
        printf("gid: %d\n", cred->gid);
        printf("pid: %d\n", cred->pid);
    } else {
        printf("rip\n");
    }

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    pthread_exit(NULL);
}

void* client_thread(void *arg) {
    sleep(1); 

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        pthread_exit(NULL);
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        close(sock);
        pthread_exit(NULL);
    }

        // Внутри client_thread после установки соединения:
    struct msghdr msg = {0};
    struct iovec iov;
    char message[] = "hi.";

    iov.iov_base = message;
    iov.iov_len = strlen(message);

    char cmsg_buf[CMSG_SPACE(sizeof(struct ucred))];
    struct cmsghdr *cmsg;

    struct ucred cred;
    cred.pid = getpid();  // текущий pid
    cred.uid = getuid();  // текущий uid
    cred.gid = getgid();  // текущий gid

    // Формируем управляющее сообщение с учетными данными
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = CMSG_SPACE(sizeof(struct ucred));

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_CREDENTIALS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));

    memcpy(CMSG_DATA(cmsg), &cred, sizeof(struct ucred));

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (sendmsg(sock, &msg, 0) == -1) {
        perror("sendmsg");
    }
    printf("send manual creds\n");
    close(sock);
    pthread_exit(NULL);
}

int main() {
    pthread_t server_tid, client_tid;

    if (pthread_create(&server_tid, NULL, server_thread, NULL) != 0) {
        perror("pthread_create server");
        return 1;
    }

    if (pthread_create(&client_tid, NULL, client_thread, NULL) != 0) {
        perror("pthread_create client");
        return 1;
    }

    pthread_join(server_tid, NULL);
    pthread_join(client_tid, NULL);

    return 0;
}