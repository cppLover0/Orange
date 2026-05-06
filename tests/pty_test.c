#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pty.h>
#include <utmp.h>

int main() {
    int master_fd, slave_fd;
    char slave_path[256];

    if (openpty(&master_fd, &slave_fd, slave_path, NULL, NULL) == -1) {
        perror("openpty");
        return 1;
    }

    printf("Master FD: %d\n", master_fd);
    printf("Slave FD: %d\n", slave_fd);
    printf("Slave path: %s\n", slave_path);

    const char *data = "Hello via PTY\n";
    write(master_fd, data, 14);

    char buffer[14];
    ssize_t n = read(slave_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Read from slave: %s\n", buffer);
    }

    close(master_fd);
    close(slave_fd);

    return 0;
}
