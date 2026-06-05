/*
 * test_edf.c — Earliest Deadline First demo
 *
 * Three periodic tasks with different periods and deadlines:
 *
 *   Task A: period 200ms, deadline 180ms  (tight)
 *   Task B: period 500ms, deadline 450ms  (medium)
 *   Task C: period 300ms, deadline 280ms  (medium-tight)
 *
 * Without EDF: C might miss its deadline because A hogs the CPU.
 * With EDF:    the scheduler always runs whichever deadline is soonest.
 *
 * Each task also deliberately does NOT yield for 30ms to test that
 * real preemption kicks in and reschedules correctly.
 */

#include <stdio.h>
#include <time.h>
#include "rtos_threaded.h"

static long long ms_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Task A — tight deadline, 200ms period */
void task_edf_a(void) {
    for (int i = 0; i < 4; i++) {
        long long start = ms_now();
        printf("[Task A] Iteration %d — started, deadline: %lldms from boot\n",
               i+1, process_table[current_process].deadline_ms);

        /* 20ms of "work" — no yield, testing preemption */
        struct timespec busy = {0, 20000000};
        nanosleep(&busy, NULL);

        printf("[Task A] Iteration %d — done (took ~20ms)\n", i+1);

        /* Renew deadline for next period */
        os_set_deadline(ms_now() + 180);
        os_delay(180);   /* sleep until next period */
    }
    printf("[Task A] All iterations complete.\n");
}

/* Task B — relaxed deadline, 500ms period */
void task_edf_b(void) {
    for (int i = 0; i < 2; i++) {
        printf("[Task B] Iteration %d — started, deadline: %lldms from boot\n",
               i+1, process_table[current_process].deadline_ms);

        struct timespec busy = {0, 30000000};  /* 30ms work */
        nanosleep(&busy, NULL);

        printf("[Task B] Iteration %d — done.\n", i+1);
        os_set_deadline(ms_now() + 450);
        os_delay(450);
    }
    printf("[Task B] All iterations complete.\n");
}

/* Task C — medium deadline, 300ms period */
void task_edf_c(void) {
    for (int i = 0; i < 3; i++) {
        printf("[Task C] Iteration %d — started, deadline: %lldms from boot\n",
               i+1, process_table[current_process].deadline_ms);

        struct timespec busy = {0, 15000000};  /* 15ms work */
        nanosleep(&busy, NULL);

        printf("[Task C] Iteration %d — done.\n", i+1);
        os_set_deadline(ms_now() + 280);
        os_delay(280);
    }
    printf("[Task C] All iterations complete.\n");
}

#ifndef CLI_BUILD
int main(void) {
    printf("=== EDF Scheduling Demo ===\n");
    printf("3 periodic tasks: A(180ms deadline), B(450ms), C(280ms)\n");
    printf("Preemption every %dms — tasks run without yielding\n\n", 10);

    os_init();
    os_set_policy(SCHED_EDF);

    create_process_edf(task_edf_a, 180, 200);
    create_process_edf(task_edf_b, 450, 500);
    create_process_edf(task_edf_c, 280, 300);

    os_start();
    return 0;
}
#endif
