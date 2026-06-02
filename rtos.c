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

// ----------MUTEX CODE-----------

void mutex_init(Mutex* m){
    m->is_locked = false;
    m->owner_id = -1;
}

void mutex_acquire(Mutex *m){
    //keep trying until lock is grabbed
    while (m->is_locked){

        // PRIORITY INHERITANCE: 
        // If we have a higher priority than the current owner, boost the owner!
        if (process_table[current_process].priority > process_table[m->owner_id].priority) {
            printf("[KERNEL] Priority Inheritance! Boosting Task %d to Priority %d\n", 
                   m->owner_id, process_table[current_process].priority);
            process_table[m->owner_id].priority = process_table[current_process].priority;
        }
        
        // Put ourselves to sleep indefinitely (-1) until the lock is released
        process_table[current_process].state = BLOCKED;
        process_table[current_process].sleep_ticks = -1; 
        os_yield();
    }
    // We got the lock!
    m->is_locked = true;
    m->owner_id = current_process;
}

void mutex_release(Mutex* m) {
    if (m->owner_id == current_process) {
        m->is_locked = false;
        m->owner_id = -1;
        
        // Restore our original priority in case we were boosted
        process_table[current_process].priority = process_table[current_process].base_priority;
        
        // Wake up ANY tasks that were blocked indefinitely waiting for this mutex
        for (int i = 0; i < process_count; i++) {
            if (process_table[i].state == BLOCKED && process_table[i].sleep_ticks == -1) {
                process_table[i].state = READY;
            }
        }
        
        // Yield the CPU so the highest priority awoken task can immediately grab the lock
        os_yield(); 
    }
}


void mq_init(MessageQueue* q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    mutex_init(&q->lock);
}

void mq_send(MessageQueue* q, int data) {
    while (1) {
        mutex_acquire(&q->lock);
        
        // If there is space in the queue, add the data!
        if (q->count < QUEUE_MAX_SIZE) {
            q->buffer[q->tail] = data;
            q->tail = (q->tail + 1) % QUEUE_MAX_SIZE;
            q->count++;
            mutex_release(&q->lock);
            return; // Success, exit the function
        }
        
        // If the queue is full, release the lock and yield the CPU 
        // to let the Consumer run and empty out some space.
        mutex_release(&q->lock);
        os_yield(); 
    }
}

int mq_receive(MessageQueue* q) {
    int data;
    while (1) {
        mutex_acquire(&q->lock);
        
        // If there is data in the queue, grab it!
        if (q->count > 0) {
            data = q->buffer[q->head];
            q->head = (q->head + 1) % QUEUE_MAX_SIZE;
            q->count--;
            mutex_release(&q->lock);
            return data; // Success, return the data
        }
        
        // If the queue is empty, release the lock and sleep for 1 tick.
        // This ensures the Consumer doesn't hog the CPU while waiting for data!
        mutex_release(&q->lock);
        os_delay(10); 
    }
}

// ------------NEW: SEMAPHORE FUNCTIONALITY-----------------

void sem_init(Semaphore* s, int max) {
    s->count = max;
    s->max_count = max;
    mutex_init(&s->lock); // Initialize the underlying mutex
}

// Equivalent to acquiring a resource
void sem_wait(Semaphore* s) {
    while (1) {
        // Lock the semaphore safely to check the count
        mutex_acquire(&s->lock);
        
        if (s->count > 0) {
            // A resource is available! Take it.
            s->count--;
            mutex_release(&s->lock);
            return; 
        }
        
        // If the pool is empty (count == 0), unlock and go to sleep.
        // We delay so the tasks currently holding resources can finish!
        mutex_release(&s->lock);
        os_delay(5); 
    }
}

// Equivalent to releasing a resource
void sem_post(Semaphore* s) {
    mutex_acquire(&s->lock);
    
    // Only return a resource if we haven't exceeded the maximum capacity
    if (s->count < s->max_count) {
        s->count++;
    }
    
    mutex_release(&s->lock);
}

void os_start(void) {
    printf("Starting Lightweight RTOS Kernel...\n");

    while (1) {
        int active_tasks = 0;

        // THE SYSTEM TICK (Time Management)
        Sleep(1); // Standard Windows sleep to slow down our simulation
        
        for (int i = 0; i < process_count; i++) {
            // If a task is blocked and waiting on a timer, decrement its timer
            if (process_table[i].state == BLOCKED && process_table[i].sleep_ticks > 0) {
                process_table[i].sleep_ticks--;
                
                // If the timer hits zero, wake the task up!
                if (process_table[i].sleep_ticks == 0) {
                    process_table[i].state = READY;
                }
            }
        }
        
        // --- STRICT PRIORITY SCHEDULER ---
        int next_process = -1;
        int highest_priority = -1;

        // Scan all processes to find the highest priority task that is ready to run
        for (int i = 0; i < process_count; i++) {
            if (process_table[i].state == READY || process_table[i].state == RUNNING) {
                // If we find a process with a higher priority, select it
                if (process_table[i].priority > highest_priority) {
                    highest_priority = process_table[i].priority;
                    next_process = i;
                }
            }
        }

        // If we found a valid task to run
        if (next_process != -1) {
            current_process = next_process;
            process_table[current_process].state = RUNNING;
            active_tasks++;

            // CONTEXT SWITCH
            SwitchToFiber(process_table[current_process].fiber);
        }

        // If tasks are BLOCKED, we still count them as active so the OS doesn't shut down
        for (int i = 0; i < process_count; i++) {
            if (process_table[i].state == BLOCKED) {
                active_tasks++;
            }
        }

        // If no tasks are READY or RUNNING, exit the kernel loop
        if (active_tasks == 0) {
            printf("All processes terminated. Shutting down OS.\n");
            break;
        }
    }
}

//Puts the current running task to sleep for a set number of ticks
void os_delay(int ticks) {
    if (current_process != -1 && ticks > 0) {
        process_table[current_process].sleep_ticks = ticks;
        process_table[current_process].state = BLOCKED;
        os_yield(); // Hand control back to the OS immediately
    }
}