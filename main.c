/* Application - Master RTOS Demonstration */

#include <stdio.h>
#include <windows.h>
#include "rtos.h"

// 1. Background Task (Priority 1)
// Simulates continuous low-level work, like reading an ambient sensor.
void background_task(void) {
    int counter = 0;
    while (counter < 10) {
        printf("[Time: %5lu ms]  [LOW]    Background Task processing data (Step %d)...\n", GetTickCount(), counter + 1);
        counter++;
        
        // Yields time so it doesn't spam the console too fast
        os_delay(10); 
    }
    printf("[Time: %5lu ms]  [LOW]    Background Task finished.\n", GetTickCount());
}

// 2. Periodic Task (Priority 3)
// Simulates an important routine task, like sending data over Wi-Fi.
void periodic_task(void) {
    int counter = 0;
    while (counter < 4) {
        printf("[Time: %5lu ms]  [MED]  --> Periodic Task woke up! Transmitting data.\n", GetTickCount());
        counter++;
        
        // Sleeps for longer, giving the background task plenty of time to run
        os_delay(30); 
    }
    printf("[Time: %5lu ms]  [MED]  --> Periodic Task finished.\n", GetTickCount());
}

// 3. Critical Task (Priority 5)
// Simulates an emergency interrupt that must take absolute control.
void critical_task(void) {
    int counter = 0;
    while (counter < 2) {
        printf("[Time: %5lu ms]  [HIGH] CRITICAL EVENT INTERCEPT!\n", GetTickCount());
        counter++;
        
        // Sleeps for a very long time, lurking in the background until needed
        os_delay(80); 
    }
    printf("[Time: %5lu ms]  [HIGH] Critical Task finished.\n", GetTickCount());
}

int main() {
    printf("===================================================\n");
    printf("      Booting Lightweight RTOS Simulation          \n");
    printf("===================================================\n");
    
    os_init();
    
    printf("Registering tasks with the Scheduler...\n\n");
    
    // Create tasks with strictly defined priorities
    create_process(background_task, 1); // Priority 1 (Lowest)
    create_process(periodic_task, 3);   // Priority 3 (Medium)
    create_process(critical_task, 5);   // Priority 5 (Highest)

    // Hand over control from Windows to our RTOS Kernel
    os_start();
    
    printf("\n===================================================\n");
    printf("      System Shutdown Complete.                    \n");
    printf("===================================================\n");
    
    return 0;
}