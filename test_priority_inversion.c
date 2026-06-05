#include <stdio.h>
#include "rtos_threaded.h"

Mutex shared_resource;

// Task 1: Low Priority (Priority 1)
void task_low(void) {
    printf("[LOW]  Trying to acquire Mutex...\n");
    mutex_acquire(&shared_resource);
    printf("[LOW]  Mutex Acquired! Doing slow work...\n");
    
    // We run for 40 ticks so Medium and High have time to wake up and attack!
    for(int i = 0; i < 40; i++) {
        // Only print every 10 ticks so we don't spam the console
        if (i % 10 == 0) {
            printf("[LOW]  Working inside Mutex... (My Current Priority: %d)\n", process_table[current_process].priority);
        }
        os_yield(); // 1 tick passes
    }
    
    printf("[LOW]  Finished work. Releasing Mutex.\n");
    mutex_release(&shared_resource);
    printf("[LOW]  Task complete.\n");
}

// Task 2: Medium Priority (Priority 3)
void task_medium(void) {
    // Wake up at exactly 10 ticks
    os_delay(10); 
    
    printf("[MED]  ---> Medium Task woke up! Trying to interrupt Low Task...\n");
    
    // Hog the CPU for 20 ticks. This causes Priority Inversion!
    for(int i = 0; i < 20; i++) {
        if (i % 5 == 0) {
            printf("[MED]  Medium task is hogging the CPU!\n");
        }
        os_yield(); // 1 tick passes
    }
    printf("[MED]  Task complete.\n");
}

// Task 3: High Priority (Priority 5)
void task_high(void) {
    // Wake up at exactly 20 ticks
    os_delay(20); 
    
    printf("[HIGH] =====> High Task woke up! I need the Mutex!\n");
    mutex_acquire(&shared_resource); 
    
    printf("[HIGH] =====> Mutex Acquired! Critical work done.\n");
    mutex_release(&shared_resource);
    printf("[HIGH] =====> Task complete.\n");
}

#ifndef CLI_BUILD
int main() {
    os_init();
    mutex_init(&shared_resource);
    
    create_process(task_low, 1);    
    create_process(task_medium, 3); 
    create_process(task_high, 5);   

    printf("Booting OS with Priority Inheritance...\n");
    os_start();
    return 0;
}}
#endif /* CLI_BUILD */
