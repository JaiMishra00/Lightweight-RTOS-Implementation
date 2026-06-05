#ifndef RTOS_THREADED_H
#define RTOS_THREADED_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#define MAX_PROCESSES  8
#define QUEUE_MAX_SIZE 5

/* ── Scheduler policy ───────────────────────────────────────────────────── */
typedef enum {
    SCHED_PRIORITY,   /* strict highest-priority-first (original) */
    SCHED_EDF,        /* earliest-deadline-first                  */
} SchedPolicy;

/* ── Process state ──────────────────────────────────────────────────────── */
typedef enum { READY, RUNNING, BLOCKED, TERMINATED } ProcessState;

/* ── PCB ────────────────────────────────────────────────────────────────── */
typedef struct {
    int          id;
    int          priority;
    int          base_priority;
    ProcessState state;
    void       (*entry_point)(void);

    pthread_t    thread;
    sem_t        run_token;      /* scheduler posts this to let task run  */
    int          sleep_ticks;    /* >0 timed sleep, -1 mutex-blocked      */

    /* EDF fields */
    long long    deadline_ms;    /* absolute deadline in ms since boot     */
    long long    period_ms;      /* task period; 0 = aperiodic            */
    int          missed_deadlines;
} PCB;

/* ── Mutex ──────────────────────────────────────────────────────────────── */
typedef struct {
    int              owner_id;
    bool             is_locked;
    pthread_mutex_t  internal;
} Mutex;

/* ── Message Queue ──────────────────────────────────────────────────────── */
typedef struct {
    int    buffer[QUEUE_MAX_SIZE];
    int    head, tail, count;
    Mutex  lock;
} MessageQueue;

/* ── Counting Semaphore ─────────────────────────────────────────────────── */
typedef struct {
    int    count;
    int    max_count;
    Mutex  lock;
} Semaphore;

/* ── Kernel API ─────────────────────────────────────────────────────────── */
void  os_init(void);
void  os_set_policy(SchedPolicy policy);
int   create_process(void (*entry_point)(void), int priority);
int   create_process_edf(void (*entry_point)(void), long long deadline_ms, long long period_ms);
void  os_start(void);
void  os_yield(void);
void  os_delay(int ticks);
void  os_set_deadline(long long deadline_ms);   /* update own deadline at runtime */

/* ── Mutex API ──────────────────────────────────────────────────────────── */
void  mutex_init(Mutex *m);
void  mutex_acquire(Mutex *m);
void  mutex_release(Mutex *m);

/* ── Message Queue API ──────────────────────────────────────────────────── */
void  mq_init(MessageQueue *q);
void  mq_send(MessageQueue *q, int data);
int   mq_receive(MessageQueue *q);

/* ── RTOS Semaphore API ─────────────────────────────────────────────────── */
void  rtos_sem_init(Semaphore *s, int max);
void  rtos_sem_wait(Semaphore *s);
void  rtos_sem_post(Semaphore *s);

/* Compat macros so test files compile unchanged */
#define sem_init(s, max)  rtos_sem_init(s, max)
#define sem_wait(s)       rtos_sem_wait(s)
#define sem_post(s)       rtos_sem_post(s)

/* ── Globals ────────────────────────────────────────────────────────────── */
extern PCB        process_table[MAX_PROCESSES];
extern int        current_process;
extern int        process_count;
extern SchedPolicy sched_policy;

#endif /* RTOS_THREADED_H */
