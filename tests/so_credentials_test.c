#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>

struct un_ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

int main() {
    int sv[2];
    pid_t pid;
    struct un_ucred cred;
    socklen_t len;
    char buf[64];
    
    printf("=== Testing SO_CREDENTIALS (your implementation) ===\n\n");
    
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        perror("socketpair");
        return 1;
    }
    
    pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    
    if (pid == 0) {
        /* Child process - client */
        close(sv[0]);
        
        printf("Client: pid=%d uid=%d gid=%d\n", 
               getpid(), getuid(), getgid());
        
        strcpy(buf, "HELLO");
        if (write(sv[1], buf, strlen(buf) + 1) == -1) {
            perror("write");
            exit(1);
        }
        
        printf("Client: sent '%s'\n", buf);
        close(sv[1]);
        exit(0);
    } else {
        /* Parent process - server */
        close(sv[1]);
        
        sleep(1);
        
        len = sizeof(cred);
        if (getsockopt(sv[0], SOL_SOCKET, 17, &cred, &len) == -1) {
            perror("getsockopt SO_CREDENTIALS");
            close(sv[0]);
            wait(NULL);
            return 1;
        }
        
        printf("\nServer: SO_CREDENTIALS returned:\n");
        printf("  pid = %d (expected ~%d)\n", cred.pid, pid);
        printf("  uid = %d (expected %d)\n", cred.uid, getuid());
        printf("  gid = %d (expected %d)\n", cred.gid, getgid());
        
        if (read(sv[0], buf, sizeof(buf)) == -1) {
            perror("read");
        } else {
            printf("  message = '%s'\n", buf);
        }
        
        if (cred.pid == pid && cred.uid == getuid() && cred.gid == getgid()) {
            printf("\nSUCCESS: SO_CREDENTIALS works correctly!\n");
        } else {
            printf("\nx FAILURE: SO_CREDENTIALS returned wrong values!\n");
            printf("  This explains why D-Bus rejects EXTERNAL auth.\n");
        }
        
        close(sv[0]);
        wait(NULL);
    }
    
    return 0;
}