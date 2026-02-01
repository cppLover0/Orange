#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile sig_atomic_t real_count = 0;
volatile sig_atomic_t virt_count = 0;
volatile sig_atomic_t prof_count = 0;

void real_timer_handler(int signum) {
    real_count++;
    printf("REAL timer tick: %d\n", real_count);
}

void virt_timer_handler(int signum) {
    virt_count++;
    printf("VIRTUAL timer tick: %d (process CPU time)\n", virt_count);
}

void prof_timer_handler(int signum) {
    prof_count++;
    printf("PROF timer tick: %d (process + system CPU time)\n", prof_count);
}

int main() {
    struct sigaction sa;
    struct itimerval real_timer, virt_timer, prof_timer;
    
    // Set up handlers for different timer types
    memset(&sa, 0, sizeof(sa));
    
    // REAL timer (wall-clock time)
    sa.sa_handler = &real_timer_handler;
    sigaction(SIGALRM, &sa, NULL);
    
    // VIRTUAL timer (process CPU time)
    sa.sa_handler = &virt_timer_handler;
    sigaction(SIGVTALRM, &sa, NULL);
    
    // PROF timer (process + system CPU time)
    sa.sa_handler = &prof_timer_handler;
    sigaction(SIGPROF, &sa, NULL);
    
    // Configure REAL timer (wall-clock)
    real_timer.it_value.tv_sec = 1;
    real_timer.it_value.tv_usec = 0;
    real_timer.it_interval.tv_sec = 2;
    real_timer.it_interval.tv_usec = 0;
    
    // Configure VIRTUAL timer (user CPU time)
    virt_timer.it_value.tv_sec = 0;
    virt_timer.it_value.tv_usec = 500000;  // 0.5 seconds
    virt_timer.it_interval.tv_sec = 0;
    virt_timer.it_interval.tv_usec = 500000;
    
    // Configure PROF timer (total CPU time)
    prof_timer.it_value.tv_sec = 0;
    prof_timer.it_value.tv_usec = 300000;  // 0.3 seconds
    prof_timer.it_interval.tv_sec = 0;
    prof_timer.it_interval.tv_usec = 300000;
    
    printf("Starting three different timers:\n");
    printf("1. REAL timer: every 2 seconds (SIGALRM)\n");
    printf("2. VIRTUAL timer: every 0.5 seconds of process CPU time (SIGVTALRM)\n");
    printf("3. PROF timer: every 0.3 seconds of total CPU time (SIGPROF)\n");
    
    // Start all timers
    if (setitimer(ITIMER_REAL, &real_timer, NULL) == -1) {
        perror("setitimer REAL");
        return 1;
    }
    
    if (setitimer(ITIMER_VIRTUAL, &virt_timer, NULL) == -1) {
        perror("setitimer VIRTUAL");
        return 1;
    }
    
    if (setitimer(ITIMER_PROF, &prof_timer, NULL) == -1) {
        perror("setitimer PROF");
        return 1;
    }
    
    // Simulate some CPU work to trigger VIRTUAL and PROF timers
    printf("\nDoing CPU-intensive work...\n");
    volatile long i;
    while(1) {
        asm volatile("nop");
    }

    printf("\nCPU work completed.\n");
    printf("Final counts:\n");
    printf("  REAL timer ticks: %d\n", real_count);
    printf("  VIRTUAL timer ticks: %d\n", virt_count);
    printf("  PROF timer ticks: %d\n", prof_count);
    
    return 0;
}