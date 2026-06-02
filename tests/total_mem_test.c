#include <stdio.h>
#include <sys/sysinfo.h>

int main() {
    struct sysinfo info;

    if (sysinfo(&info) != 0) {
        perror("Error calling sysinfo");
        return 1;
    }

    long long total_mem_mb = (long long)info.totalram / (1024 * 1024);
    long long free_mem_mb = (long long)info.freeram / (1024 * 1024);

    printf("Total RAM: %lld MB\n", total_mem_mb);
    printf("Free RAM: %lld MB\n", free_mem_mb);

    return 0;
}
