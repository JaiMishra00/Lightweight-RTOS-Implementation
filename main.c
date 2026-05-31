/* Application */

#include <stdio.h>
#include "rtos.h"

//simulated process 1
void process_one(void){
    for (int i = 0; i < 3; i++){
        printf("Process 1 is executing (step %d)\n", i + 1);
        os_yield(); //hand control back to OS
    }
    printf("Process 1 is finished\n");
}

//simulated process 2
void process_two(void){
    for (int i = 0; i < 3; i++){
        printf("Process 2 is executing (step %d)\n", i + 1);
        os_yield(); //hand control back to OS
    }
    printf("Process 2 is finished\n");
}

int main(){
    printf("initializing os\n");
    os_init();
    
    printf("creating processes\n");
    create_process(process_one, 1);
    create_process(process_two, 1);

    printf("booting os\n");
    os_start();
    return 0;

}

