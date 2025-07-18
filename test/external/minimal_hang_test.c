/*
 * Minimal test to isolate the hang issue
 * This bypasses all the complex logic and tests just the core lock acquisition
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

/* Global flag for signal handling */
volatile int should_exit = 0;

void signal_handler(int sig) {
    should_exit = 1;
}

int main() {
    struct timeval start_time, now;
    double elapsed;
    double timeout = 0.1;
    
    printf("Starting minimal hang test...\n");
    
    /* Install simple signal handler */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    printf("Signal handlers installed\n");
    
    /* Start timing */
    gettimeofday(&start_time, NULL);
    printf("Timer started\n");
    
    /* Simple timeout loop */
    while (!should_exit) {
        gettimeofday(&now, NULL);
        elapsed = (now.tv_sec - start_time.tv_sec) + 
                 (now.tv_usec - start_time.tv_usec) / 1000000.0;
        
        printf("Elapsed: %.3f seconds\n", elapsed);
        
        if (elapsed >= timeout) {
            printf("Timeout reached: %.3f >= %.3f\n", elapsed, timeout);
            break;
        }
        
        /* Sleep for 10ms */
        usleep(10000);
    }
    
    printf("Test completed\n");
    return 0;
}