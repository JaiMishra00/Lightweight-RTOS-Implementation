#include <stdio.h>
#include "rtos.h"

// Simulated Process 1: Runs every 500 ticks
void process_one(void) {
    int counter = 0;
    while (counter < 3) { // Let's stop after 3 runs so the program can exit gracefully
        printf("[Time: %d ms] Process 1 executing (Tick Run %d)\n", GetTickCount(), counter + 1);
        counter++;
        
        os_delay(500); // Sleep for 500 "ticks" (milliseconds)
    }
    printf("Process 1 is permanently finished.\n");
}

// Simulated Process 2: Runs every 1000 ticks (Slower)
void process_two(void) {
    int counter = 0;
    while (counter < 3) {
        printf("[Time: %d ms] \t\tProcess 2 executing (Tick Run %d)\n", GetTickCount(), counter + 1);
        counter++;
        
        os_delay(1000); // Sleep for 1000 "ticks"
    }
    printf("Process 2 is permanently finished.\n");
}

int main() {
    printf("Initializing OS...\n");
    os_init();
    
    printf("Creating processes...\n");
    create_process(process_one, 1);
    create_process(process_two, 1);

    printf("Booting OS...\n");
    os_start();
    return 0;
}