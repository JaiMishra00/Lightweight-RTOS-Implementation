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
    int base_priority; // prevents priority inversion by storing the original priority
    ProcessState state;
    LPVOID fiber;              // Windows Fiber pointer (Our CPU Context)
    void (*entry_point)(void); 
    int sleep_ticks; // tracks how long the process is asleep [-1 implies blocked indefinitely and requires lock]
} PCB;

// NEW: Mutex Structure
typedef struct {
    int owner_id;       // Which process holds the lock (-1 if free)
    bool is_locked;     // True if currently held
} Mutex;

// Kernel API
void os_init(void);
int create_process(void (*entry_point)(void), int priority);
void os_start(void);
void os_yield(void);
void os_delay(int ticks); //puts task to sleep

// NEW: Mutex API
void mutex_init(Mutex* m);
void mutex_acquire(Mutex* m);
void mutex_release(Mutex* m);

extern PCB process_table[MAX_PROCESSES];
extern int current_process;

#endif // RTOS_H