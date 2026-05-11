#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    uid_t ruid = getuid();
    uid_t euid = geteuid();

    printf("Client-side IDs -> Real UID: %u (0x%x), Effective UID: %u (0x%x)\n",
           ruid, ruid, euid, euid);

    if (euid == 4294967295U) {
        printf("ERROR: geteuid() returned (uid_t)-1, which is invalid.\n");
    } else {
        char uid_str[16];
        int len = snprintf(uid_str, sizeof(uid_str), "%u", euid);
        printf("String representation: \"%s\"\n", uid_str);
        printf("Hex encoding for AUTH EXTERNAL: ");
        for (int i = 0; i < len; i++) {
            printf("%x", uid_str[i]);
        }
        printf("\n");
    }
    return 0;
}