#include <stdio.h>
#include "rtos.h"

PCB process_table[MAX_PROCESSES];
int current_process = -1;
int process_count = 0;
LPVOID kernel_fiber; // Saves the OS scheduler's context

// This wrapper acts as the bridge between the Fiber and our C function
VOID WINAPI fiber_wrapper(LPVOID param) {
    int id = (int)(intptr_t)param;
    
    // Run the actual process function
    process_table[id].entry_point();
    
    // If the function returns, mark it terminated and yield back to OS
    process_table[id].state = TERMINATED;
    SwitchToFiber(kernel_fiber);
}

void os_init(void) {
    // Convert the main Windows thread into a Fiber so we can switch back to it
    kernel_fiber = ConvertThreadToFiber(NULL);
    process_count = 0;
    current_process = -1;
}

int create_process(void (*entry_point)(void), int priority) {
    if (process_count >= MAX_PROCESSES) {
        printf("Error: Process table full!\n");
        return -1; 
    }

    int id = process_count;
    process_table[id].id = id;
    process_table[id].priority = priority;
    process_table[id].base_priority = priority;
    process_table[id].state = READY;
    process_table[id].entry_point = entry_point;
    
    // Create the Fiber with its own isolated memory stack!
    process_table[id].fiber = CreateFiber(STACK_SIZE, fiber_wrapper, (LPVOID)(intptr_t)id);

    process_count++;
    return id;
}

void os_yield(void) {
    // Switch CPU context back to the Kernel Scheduler
    SwitchToFiber(kernel_fiber);
}

void os_start(void) {
    printf("Starting Lightweight RTOS Kernel...\n");

    while (1) {
        int active_tasks = 0;
        
        // Simple Round Robin Scheduling logic
        current_process = (current_process + 1) % process_count;

        // Check if the process is valid to run
        if (process_table[current_process].state == READY ||
            process_table[current_process].state == RUNNING) {
            
            process_table[current_process].state = RUNNING;
            active_tasks++;

            // CONTEXT SWITCH: Jump to the process's Fiber
            SwitchToFiber(process_table[current_process].fiber);
        }

        // If no tasks are READY or RUNNING, exit the kernel loop
        if (active_tasks == 0) {
            printf("All processes terminated. Shutting down OS.\n");
            break;
        }
    }
}