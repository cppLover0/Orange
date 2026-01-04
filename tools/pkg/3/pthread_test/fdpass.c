#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>

#define THREAD_COUNT 2

// Shared socket pair file descriptors
int socket_pair[2];

// Thread to send a file descriptor
void* sender_thread(void* arg) {
    // Open a file to pass its descriptor
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }

    // Prepare message to send
    struct msghdr msg = {0};
    char buf[CMSG_SPACE(sizeof(int))];
    memset(buf, 0, sizeof(buf));
    struct iovec io = {.iov_base = (void*)"FD", .iov_len = 2};

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    // Set control message to pass the file descriptor
    struct cmsghdr* cmsg = (struct cmsghdr*)buf;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    // Copy the fd into the control message
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    msg.msg_control = cmsg;
    msg.msg_controllen = CMSG_LEN(sizeof(int));

    // Send message with the file descriptor
    if (sendmsg(socket_pair[0], &msg, 0) < 0) {
        perror("sendmsg");
    } else {
        printf("Sender: sent file descriptor\n");
    }

    close(fd);
    return NULL;
}

// Thread to receive a file descriptor
void* receiver_thread(void* arg) {
    char buf[1];
    struct msghdr msg = {0};
    char ctrl_buf[CMSG_SPACE(sizeof(int))];

    struct iovec io = {.iov_base = buf, .iov_len = 1};
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl_buf;
    msg.msg_controllen = sizeof(ctrl_buf);

    // Receive message and ancillary data
    if (recvmsg(socket_pair[1], &msg, 0) < 0) {
        perror("recvmsg");
        return NULL;
    }

    // Extract the file descriptor from the control message
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    int received_fd = -1;

    if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
        printf("Receiver: received file descriptor %d\n", received_fd);

        // Read from the received file descriptor
        char read_buf[128];
        ssize_t n = read(received_fd, read_buf, sizeof(read_buf)-1);
        if (n > 0) {
            read_buf[n] = '\0';
            printf("Content of the file: %s\n", read_buf);
        } else {
            perror("read");
        }

        close(received_fd);
    } else {
        printf("No file descriptor received (size %d)\n",0);
    }
    return NULL;
}

int main() {
    pthread_t threads[THREAD_COUNT];

    // Create a socket pair for communication
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socket_pair) < 0) {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }

    // Create sender thread
    if (pthread_create(&threads[0], NULL, sender_thread, NULL) != 0) {
        perror("pthread_create sender");
        exit(EXIT_FAILURE);
    }

    // Create receiver thread
    if (pthread_create(&threads[1], NULL, receiver_thread, NULL) != 0) {
        perror("pthread_create receiver");
        exit(EXIT_FAILURE);
    }

    // Wait for threads to finish
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    close(socket_pair[0]);
    close(socket_pair[1]);

    return 0;
}