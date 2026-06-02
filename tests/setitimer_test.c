#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

static volatile sig_atomic_t real_count = 0;
static volatile sig_atomic_t virtual_count = 0;
static volatile sig_atomic_t prof_count = 0;

static void handle_sigalrm(int sig) {
    (void)sig;
    real_count++;
    write(STDOUT_FILENO, "[SIGNAL] ITIMER_REAL triggered\n", 31);
}

static void handle_sigvtalrm(int sig) {
    (void)sig;
    virtual_count++;
    write(STDOUT_FILENO, "[SIGNAL] ITIMER_VIRTUAL triggered\n", 34);
}

static void handle_sigprof(int sig) {
    (void)sig;
    prof_count++;
    write(STDOUT_FILENO, "[SIGNAL] ITIMER_PROF triggered\n", 31);
}

static void setup_signal(int sig, void (*handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

static void set_timer(int which, long tv_sec, long tv_usec, long interval_sec, long interval_usec) {
    struct itimerval it;
    it.it_value.tv_sec = tv_sec;
    it.it_value.tv_usec = tv_usec;
    it.it_interval.tv_sec = interval_sec;
    it.it_interval.tv_usec = interval_usec;
    if (setitimer(which, &it, NULL) == -1) {
        perror("setitimer");
        exit(EXIT_FAILURE);
    }
}

static void disable_timer(int which) {
    set_timer(which, 0, 0, 0, 0);
}

static void consume_cpu(void) {
    volatile unsigned long long i;
    for (i = 0; i < 10000000ULL; i++);
}

int main(void) {
    setup_signal(SIGALRM, handle_sigalrm);
    setup_signal(SIGVTALRM, handle_sigvtalrm);
    setup_signal(SIGPROF, handle_sigprof);

    printf("=== Test Periodic ITIMER_REAL ===\n");
    set_timer(ITIMER_REAL, 0, 10000, 0, 10000);
    while (real_count < 5) {
        usleep(1000);
    }
    disable_timer(ITIMER_REAL);
    printf("PASSED (triggered %d times)\n\n", real_count);

    printf("=== Test Periodic ITIMER_VIRTUAL ===\n");
    set_timer(ITIMER_VIRTUAL, 0, 10000, 0, 10000);
    while (virtual_count < 5) {
        consume_cpu();
    }
    disable_timer(ITIMER_VIRTUAL);
    printf("PASSED (triggered %d times)\n\n", virtual_count);

    printf("=== Test Periodic ITIMER_PROF ===\n");
    set_timer(ITIMER_PROF, 0, 10000, 0, 10000);
    while (prof_count < 5) {
        consume_cpu();
    }
    disable_timer(ITIMER_PROF);
    printf("PASSED (triggered %d times)\n\n", prof_count);

    printf("=== Test Continuous Timers Success ===\n");
    return EXIT_SUCCESS;
}
