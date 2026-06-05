#include <stdio.h>
#include "rtos_threaded.h"

Semaphore parking_lot;

// The simulated task (5 of these will be created)
void car_task(void) {
    // We use the OS's current_process ID just to give each car a unique name in the logs
    int car_id = current_process; 
    
    printf("[Time: %5lu] [Car %d] Arrived at parking lot. Waiting for a spot...\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), car_id);
    
    // Ask the semaphore for permission to enter
    sem_wait(&parking_lot);
    
    printf("[Time: %5lu] [Car %d] PARKED! (Spots left: %d)\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), car_id, parking_lot.count);
    
    // Simulate being parked and doing stuff for 60 ticks
    for(int i = 0; i < 3; i++) {
        os_delay(20); 
    }
    
    printf("[Time: %5lu] [Car %d] Leaving parking lot. Freeing spot...\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), car_id);
    
    // Give the spot back to the semaphore pool
    sem_post(&parking_lot);
    
    printf("[Car %d] Drove away.\n", car_id);
}

#ifndef CLI_BUILD
int main() {
    printf("Initializing Kernel\n");
    os_init();
    
    // Initialize the semaphore with exactly 3 spots
    sem_init(&parking_lot, 3); 

    printf("Creating 5 competing tasks...\n");
    // We give them all the exact same priority (2) so they are treated equally
    create_process(car_task, 2); // Car 0
    create_process(car_task, 2); // Car 1
    create_process(car_task, 2); // Car 2
    create_process(car_task, 2); // Car 3
    create_process(car_task, 2); // Car 4

    printf("Booting OS...\n\n");
    os_start();
    
    return 0;
}}
#endif /* CLI_BUILD */
