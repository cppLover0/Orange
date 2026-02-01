#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/example_socket"
#define BUFFER_SIZE 1024

void run_server() {
    printf("Starting server...\n");
    
    int server_fd, client_fd;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Create socket
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Remove old socket file if it exists
    unlink(SOCKET_PATH);

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on %s\n", SOCKET_PATH);

    // Accept connections in a loop
    while (1) {
        printf("Waiting for connection...\n");
        
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        printf("Client connected\n");

        // Read data from client
        while ((bytes_read = read(client_fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            printf("Received from client: %s", buffer);
            
            // Send response
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "Server received: %s", buffer);
            write(client_fd, response, strlen(response));
            
            // If client sent "exit", close connection
            if (strncmp(buffer, "exit", 4) == 0) {
                printf("Client requested exit\n");
                break;
            }
        }

        if (bytes_read == -1) {
            perror("read");
        }

        close(client_fd);
        printf("Connection closed\n");
    }

    close(server_fd);
    unlink(SOCKET_PATH);
}

void run_client() {
    printf("Starting client...\n");
    
    int sock_fd;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Create socket
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Connect to server
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server. Enter messages (type 'exit' to quit):\n");

    // Read user input and send to server
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        // Send message
        write(sock_fd, buffer, strlen(buffer));

        // If user entered "exit", quit
        if (strncmp(buffer, "exit", 4) == 0) {
            break;
        }

        // Read response from server
        bytes_read = read(sock_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Server response: %s\n", buffer);
        } else if (bytes_read == 0) {
            printf("Server closed connection\n");
            break;
        } else {
            perror("read");
            break;
        }
    }

    close(sock_fd);
    printf("Client finished\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage:\n");
        printf("  %s server - run as server\n", argv[0]);
        printf("  %s client - run as client\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "server") == 0) {
        run_server();
    } else if (strcmp(argv[1], "client") == 0) {
        run_client();
    } else {
        printf("Unknown argument: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}