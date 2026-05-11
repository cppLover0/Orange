#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    int fd = open("t", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    
    if (fd < 0) {
        return 1; 
    }

    if (dup2(fd, 1) < 0) {
        return 2;
    }

    close(fd);

    if (write(1, "hi\n", 3) < 0) {
        return 3;
    }

    return 0;
}
