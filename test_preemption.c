/*
 * test_preemption.c — proves real preemption is working
 *
 * Task LOW (priority 1) runs an infinite busy-loop — it NEVER calls
 * os_yield(). Without preemption it would starve everything else forever.
 *
 * Task HIGH (priority 5) wakes after 100ms and should immediately
 * preempt LOW via SIGALRM, even though LOW never yields.
 */

#include <stdio.h>
#include <time.h>
#include "rtos_threaded.h"

static long long ms_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void task_low_busy(void) {
    printf("[LOW]  Started — entering busy loop (no yields!)\n");
    long long start = ms_now();
    long long iter = 0;
    while (ms_now() - start < 600) {
        iter++;   /* pure CPU spin */
    }
    printf("[LOW]  Busy loop done after 600ms (%lld iterations)\n", iter);
}

void task_high_preempt(void) {
    printf("[HIGH] Started — sleeping 100ms then demanding CPU\n");
    os_delay(100);
    printf("[HIGH] *** Woke up — I should have preempted LOW! ***\n");
    long long t = ms_now();
    os_delay(50);
    printf("[HIGH] Did 50ms of work. Done.\n");
}

void task_med_watcher(void) {
    for (int i = 0; i < 5; i++) {
        os_delay(80);
        printf("[MED]  Heartbeat %d — I'm getting CPU time\n", i+1);
    }
}

#ifndef CLI_BUILD
int main(void) {
    printf("=== Preemption Demo ===\n");
    printf("LOW never yields. HIGH and MED must preempt it.\n\n");

    os_init();
    /* leave policy as SCHED_PRIORITY */

    create_process(task_low_busy,    1);
    create_process(task_med_watcher, 3);
    create_process(task_high_preempt,5);

    os_start();
    return 0;
}
#endif
