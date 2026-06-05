#include <stdio.h>
#include "rtos_threaded.h"

// FIX: We must use a Semaphore for the database so ANY reader can unlock it! -> I used mutex before and writers were in deadlock
Semaphore resource_lock; 
Mutex rmutex;            // We can keep this a Mutex because the same reader locks and unlocks it rapidly
int read_count = 0;      

void reader_task(void) {
    int id = current_process;
    for(int i = 0; i < 3; i++) {
        os_delay(15); 

        // --- ENTRY SECTION ---
        mutex_acquire(&rmutex);     
        read_count++;               
        if (read_count == 1) {      
            sem_wait(&resource_lock); // FIRST reader locks the database
        }
        mutex_release(&rmutex);     
        
        // --- CRITICAL SECTION (READING) ---
        printf("[Time: %5lu] [Reader %d] ENTERED. Total readers inside: %d\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), id, read_count);
        os_delay(20); 
        printf("[Time: %5lu] [Reader %d] LEAVING.\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), id);

        // --- EXIT SECTION ---
        mutex_acquire(&rmutex);     
        read_count--;               
        if (read_count == 0) {      
            sem_post(&resource_lock); // LAST reader unlocks the database (This works now!)
        }
        mutex_release(&rmutex);
    }
}

void writer_task(void) {
    int id = current_process;
    for(int i = 0; i < 2; i++) {
        os_delay(40); 
        
        printf("[Time: %5lu] ---> [Writer %d] WAITING to write...\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), id);
        
        // --- ENTRY SECTION ---
        sem_wait(&resource_lock); // Wait for the semaphore to be freed by the readers
        
        // --- CRITICAL SECTION (WRITING) ---
        printf("[Time: %5lu] ---> [Writer %d] WRITING TO DATABASE! (Exclusive Access)\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), id);
        os_delay(30); 
        
        // --- EXIT SECTION ---
        printf("[Time: %5lu] ---> [Writer %d] FINISHED writing.\n", ({ struct timespec _ts; clock_gettime(CLOCK_MONOTONIC,&_ts); (unsigned long)(_ts.tv_sec*1000+_ts.tv_nsec/1000000); }), id);
        sem_post(&resource_lock);
    }
}

#ifndef CLI_BUILD
int main() {
    os_init();
    
    // Initialize the Binary Semaphore to 1
    sem_init(&resource_lock, 1);
    mutex_init(&rmutex);

    create_process(reader_task, 2); 
    create_process(reader_task, 2);
    create_process(reader_task, 2);
    create_process(writer_task, 2);
    create_process(writer_task, 2);

    printf("Booting Fixed Readers-Writers Simulation...\n\n");
    os_start();
    return 0;
}}
#endif /* CLI_BUILD */
