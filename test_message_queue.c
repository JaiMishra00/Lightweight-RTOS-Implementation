#include <stdio.h>
#include <windows.h>
#include "rtos.h"

MessageQueue sensor_queue;

// Producer Task (Priority 2 - Lower Priority)
void producer_task(void) {
    for (int i = 1; i <= 5; i++) {
        int simulated_sensor_data = i * 10; 
        
        printf("[Time: %5lu] [PRODUCER] Reading hardware sensor...\n", GetTickCount());
        
        // Send data to the queue
        mq_send(&sensor_queue, simulated_sensor_data);
        printf("[Time: %5lu] [PRODUCER] Sent data: %d. Going to sleep for 50ms.\n\n", GetTickCount(), simulated_sensor_data);
        
        // Sleep to simulate time between physical sensor readings
        os_delay(50); 
    }
    printf("[PRODUCER] Task finished.\n");
}

// Consumer Task (Priority 4 - Higher Priority)
void consumer_task(void) {
    for (int i = 0; i < 5; i++) {
        // This function will automatically sleep if the queue is empty
        int received_data = mq_receive(&sensor_queue);
        
        printf("[Time: %5lu] [CONSUMER] <--- Intercepted CPU! Received data: %d\n", GetTickCount(), received_data);
        printf("[Time: %5lu] [CONSUMER] Processing data safely... \n", GetTickCount());
        
        // Simulate a tiny bit of processing time
        os_delay(10); 
    }
    printf("[CONSUMER] Task finished.\n");
}

int main() {
    printf("Initializing Kernel and Message Queues...\n");
    os_init();
    mq_init(&sensor_queue);

    // Register tasks
    create_process(producer_task, 2);
    create_process(consumer_task, 4);

    printf("Booting OS...\n\n");
    os_start();
    
    return 0;
}