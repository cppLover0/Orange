#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

void alarm_handler(int signum) {
    printf("\nALARM! Time's up!\n");
    printf("\a");  // System beep
    exit(0);
}

int main() {
    int seconds;
    
    // Set up signal handler
    signal(SIGALRM, alarm_handler);
    
    printf("Simple Alarm Clock\n");
    printf("Enter alarm time in seconds: ");
    scanf("%d", &seconds);
    
    printf("Alarm set for %d seconds.\n", seconds);
    printf("Waiting... (Press Ctrl+C to cancel)\n");
    
    // Set alarm
    alarm(seconds);
    
    // Wait indefinitely
    while(1) {
        pause();  // Wait for signal
    }
    
    return 0;
}