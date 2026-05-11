#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    uid_t ruid, euid, suid;
    
    ruid = getuid();
    euid = geteuid();
    
    printf("getuid()  = %u (0x%08x)\n", ruid, ruid);
    printf("geteuid() = %u (0x%08x)\n", euid, euid);
    
    #ifdef __linux__
    if (getresuid(&ruid, &euid, &suid) == 0) {
        printf("getresuid(): ruid=%u (0x%08x) euid=%u (0x%08x) suid=%u (0x%08x)\n", 
               ruid, ruid, euid, euid, suid, suid);
    } else {
        perror("getresuid");
    }
    #endif
    
    if (ruid == 4294967295U || euid == 4294967295U) {
        printf("ERROR: Invalid UID detected (4294967295 = -1)\n");
        return 1;
    }
    
    if (ruid == 0 && euid == 0) {
        printf("Running as root (uid=0)\n");
    }
    
    return 0;
}
