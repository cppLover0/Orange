#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

int main() {
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd == -1) {
        printf("Failed to create timerfd: %s\n", strerror(errno));
        return 1;
    }
    printf("Timerfd created successfully.\n");

    struct itimerspec rel_ts;
    rel_ts.it_value.tv_sec = 1;
    rel_ts.it_value.tv_nsec = 0;
    rel_ts.it_interval.tv_sec = 0;
    rel_ts.it_interval.tv_nsec = 0;

    printf("Starting relative timer (1 second)...\n");
    if (timerfd_settime(tfd, 0, &rel_ts, NULL) == -1) {
        printf("Failed to set relative timer: %s\n", strerror(errno));
        close(tfd);
        return 1;
    }

    uint64_t expirations;
    ssize_t s = read(tfd, &expirations, sizeof(expirations));
    if (s != sizeof(expirations)) {
        printf("Failed to read from relative timer: %s\n", strerror(errno));
        close(tfd);
        return 1;
    }
    printf("Relative timer expired. Expirations count: %llu\n", (unsigned long long)expirations);

    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) == -1) {
        printf("Failed to get current time: %s\n", strerror(errno));
        close(tfd);
        return 1;
    }

    struct itimerspec abs_ts;
    abs_ts.it_value.tv_sec = now.tv_sec + 1;
    abs_ts.it_value.tv_nsec = now.tv_nsec;
    abs_ts.it_interval.tv_sec = 0;
    abs_ts.it_interval.tv_nsec = 0;

    printf("Starting absolute timer (current time + 1 second)...\n");
    if (timerfd_settime(tfd, TFD_TIMER_ABSTIME, &abs_ts, NULL) == -1) {
        printf("Failed to set absolute timer: %s\n", strerror(errno));
        close(tfd);
        return 1;
    }

    s = read(tfd, &expirations, sizeof(expirations));
    if (s != sizeof(expirations)) {
        printf("Failed to read from absolute timer: %s\n", strerror(errno));
        close(tfd);
        return 1;
    }
    printf("Absolute timer expired. Expirations count: %llu\n", (unsigned long long)expirations);

    printf("All tests passed successfully.\n");
    close(tfd);
    return 0;
}
