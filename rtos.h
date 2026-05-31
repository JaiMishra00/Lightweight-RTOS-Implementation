#ifndef RTOS_H
#define RTOS_H

#include <windows.h> // Required for Fibers
#include <stdbool.h>

#define MAX_PROCESSES 8
#define STACK_SIZE 4096

// Process States
typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} ProcessState;

// Process Control Block (PCB)
typedef struct {
    int id;
    int priority;
    int base_priority;
    ProcessState state;
    LPVOID fiber;              // Windows Fiber pointer (Our CPU Context)
    void (*entry_point)(void); // The actual C function the process will run
} PCB;

// Kernel API
void os_init(void);
int create_process(void (*entry_point)(void), int priority);
void os_start(void);
void os_yield(void);

#endif // RTOS_H