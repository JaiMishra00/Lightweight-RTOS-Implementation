/*
 * rtos_threaded.c  —  RTOS kernel v2
 *
 * New in v2
 * ─────────
 *  1. REAL PREEMPTION via SIGALRM
 *     A POSIX interval timer fires every PREEMPT_MS milliseconds.
 *     The signal handler posts yield_sem from inside the running task's
 *     thread, which forces the scheduler to wake up and re-evaluate
 *     priorities/deadlines — even if the task never called os_yield().
 *
 *  2. EARLIEST DEADLINE FIRST (EDF)
 *     When sched_policy == SCHED_EDF the scheduler picks the READY task
 *     whose deadline_ms is smallest (soonest).  Periodic tasks
 *     automatically get their deadline renewed each period.
 *     Missed deadlines are counted and printed at shutdown.
 *
 * How preemption works
 * ────────────────────
 *  Normal (cooperative) flow:
 *    task runs → calls os_yield() → posts yield_sem → scheduler picks next
 *
 *  Preemptive flow:
 *    SIGALRM fires in the running task's thread
 *    → preempt_handler() posts yield_sem (same as os_yield internals)
 *    → sets task state READY
 *    → scheduler wakes, picks best task, posts its run_token
 *    → preempted task re-blocks on its run_token
 *
 *  A preemption_guard mutex prevents the signal from firing while the
 *  scheduler itself is doing a context switch (avoids double-post).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <limits.h>

/* ── replicate structs without compat macros ──────────────────────────── */
#define MAX_PROCESSES  8
#define QUEUE_MAX_SIZE 5

typedef enum { SCHED_PRIORITY, SCHED_EDF } SchedPolicy;
typedef enum { READY, RUNNING, BLOCKED, TERMINATED } ProcessState;

typedef struct {
    int          id;
    int          priority;
    int          base_priority;
    ProcessState state;
    void       (*entry_point)(void);
    pthread_t    thread;
    sem_t        run_token;
    int          sleep_ticks;
    long long    deadline_ms;
    long long    period_ms;
    int          missed_deadlines;
} PCB;

typedef struct {
    int              owner_id;
    bool             is_locked;
    pthread_mutex_t  internal;
} Mutex;

typedef struct {
    int    buffer[QUEUE_MAX_SIZE];
    int    head, tail, count;
    Mutex  lock;
} MessageQueue;

typedef struct {
    int    count;
    int    max_count;
    Mutex  lock;
} Semaphore;

/* ── Globals ─────────────────────────────────────────────────────────── */
PCB        process_table[MAX_PROCESSES];
int        current_process = -1;
int        process_count   = 0;
SchedPolicy sched_policy   = SCHED_PRIORITY;

static sem_t            yield_sem;
static pthread_mutex_t  sched_lock       = PTHREAD_MUTEX_INITIALIZER;

/* Guards against preemption signal firing during context switch */
static volatile sig_atomic_t preempt_guard = 0;
volatile sig_atomic_t preemption_requested = 0;

#define TICK_NS      1000000L   /* 1 ms scheduler tick          */
#define PREEMPT_MS   10         /* preemption interval: 10 ms   */

/* ── Time helpers ────────────────────────────────────────────────────── */
static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static long long boot_ms = 0;

/* ── SIGALRM preemption handler ─────────────────────────────────────── */
/*
 * Runs in the context of whichever task thread is currently executing.
 * We just need to trigger a reschedule — post yield_sem and mark the
 * current task as READY so the scheduler can preempt it.
 */
static void preempt_handler(int sig) {
    (void)sig;
    if (preempt_guard) return;          /* in mid context-switch, skip  */
    if (current_process < 0) return;

    pthread_mutex_lock(&sched_lock);
    int me = current_process;
    if (process_table[me].state == RUNNING) {
        process_table[me].state = READY;
        pthread_mutex_unlock(&sched_lock);
        sem_post(&yield_sem);           /* wake the scheduler           */
        /* re-block on our own run_token — wait for scheduler to give
           us the CPU back (or give it to a higher-priority task)       */
        sem_wait(&process_table[me].run_token);
    } else {
        pthread_mutex_unlock(&sched_lock);
    }
}

/* ── Install the interval timer ─────────────────────────────────────── */
static void install_preemption_timer(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = preempt_handler;
    sigemptyset(&sa.sa_mask);
    /* SA_RESTART so that blocked syscalls resume after the signal */
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);

    struct itimerval timer;
    timer.it_value.tv_sec     = 0;
    timer.it_value.tv_usec    = PREEMPT_MS * 1000;
    timer.it_interval.tv_sec  = 0;
    timer.it_interval.tv_usec = PREEMPT_MS * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}

static void stop_preemption_timer(void) {
    struct itimerval timer = {0};
    setitimer(ITIMER_REAL, &timer, NULL);
}

/* ── Task thread wrapper ─────────────────────────────────────────────── */
static void *task_thread(void *arg) {
    int id = (int)(intptr_t)arg;
    sem_wait(&process_table[id].run_token);
    process_table[id].entry_point();
    pthread_mutex_lock(&sched_lock);
    process_table[id].state = TERMINATED;
    pthread_mutex_unlock(&sched_lock);
    sem_post(&yield_sem);
    return NULL;
}

/* ── os_init ─────────────────────────────────────────────────────────── */
void os_init(void) {
    memset(process_table, 0, sizeof(process_table));
    process_count   = 0;
    current_process = -1;
    sched_policy    = SCHED_PRIORITY;
    boot_ms         = now_ms();
    sem_init(&yield_sem, 0, 0);
}

void os_set_policy(SchedPolicy policy) {
    sched_policy = policy;
    printf("[KERNEL] Scheduler policy: %s\n",
           policy == SCHED_EDF ? "Earliest Deadline First (EDF)"
                                : "Strict Priority");
}

/* ── create_process ──────────────────────────────────────────────────── */
int create_process(void (*entry_point)(void), int priority) {
    if (process_count >= MAX_PROCESSES) { fprintf(stderr,"Table full\n"); return -1; }
    int id = process_count++;
    process_table[id].id              = id;
    process_table[id].priority        = priority;
    process_table[id].base_priority   = priority;
    process_table[id].state           = READY;
    process_table[id].entry_point     = entry_point;
    process_table[id].sleep_ticks     = 0;
    process_table[id].deadline_ms     = LLONG_MAX; /* no deadline by default */
    process_table[id].period_ms       = 0;
    process_table[id].missed_deadlines= 0;
    sem_init(&process_table[id].run_token, 0, 0);
    pthread_create(&process_table[id].thread, NULL, task_thread, (void *)(intptr_t)id);
    return id;
}

/* EDF variant: relative_deadline_ms from now, period 0 = one-shot */
int create_process_edf(void (*entry_point)(void),
                       long long relative_deadline_ms,
                       long long period_ms) {
    int id = create_process(entry_point, 0 /* priority unused in EDF */);
    if (id < 0) return id;
    process_table[id].deadline_ms = now_ms() + relative_deadline_ms;
    process_table[id].period_ms   = period_ms;
    return id;
}

void os_set_deadline(long long deadline_ms) {
    if (current_process >= 0)
        process_table[current_process].deadline_ms = deadline_ms;
}

/* ── os_yield ────────────────────────────────────────────────────────── */
void os_yield(void) {
    int me = current_process;
    pthread_mutex_lock(&sched_lock);
    if (process_table[me].state == RUNNING)
        process_table[me].state = READY;
    pthread_mutex_unlock(&sched_lock);
    sem_post(&yield_sem);
    sem_wait(&process_table[me].run_token);
}

/* ── os_delay ────────────────────────────────────────────────────────── */
void os_delay(int ticks) {
    if (current_process < 0 || ticks <= 0) return;
    pthread_mutex_lock(&sched_lock);
    process_table[current_process].sleep_ticks = ticks;
    process_table[current_process].state       = BLOCKED;
    pthread_mutex_unlock(&sched_lock);
    sem_post(&yield_sem);
    sem_wait(&process_table[current_process].run_token);
}

/* ── Mutex ───────────────────────────────────────────────────────────── */
void mutex_init(Mutex *m) {
    m->is_locked = false;
    m->owner_id  = -1;
    pthread_mutex_init(&m->internal, NULL);
}

void mutex_acquire(Mutex *m) {
    while (1) {
        pthread_mutex_lock(&m->internal);
        if (!m->is_locked) {
            m->is_locked = true;
            m->owner_id  = current_process;
            pthread_mutex_unlock(&m->internal);
            return;
        }
        int owner = m->owner_id;
        /* Priority inheritance (priority mode) */
        if (sched_policy == SCHED_PRIORITY && owner >= 0 &&
            process_table[current_process].priority > process_table[owner].priority) {
            printf("[KERNEL] Priority Inheritance! Boosting Task %d to %d\n",
                   owner, process_table[current_process].priority);
            process_table[owner].priority = process_table[current_process].priority;
        }
        /* Deadline inheritance (EDF mode): give owner the earlier deadline */
        if (sched_policy == SCHED_EDF && owner >= 0 &&
            process_table[current_process].deadline_ms < process_table[owner].deadline_ms) {
            printf("[KERNEL] Deadline Inheritance! Tightening Task %d deadline\n", owner);
            process_table[owner].deadline_ms = process_table[current_process].deadline_ms;
        }
        pthread_mutex_unlock(&m->internal);
        pthread_mutex_lock(&sched_lock);
        process_table[current_process].state       = BLOCKED;
        process_table[current_process].sleep_ticks = -1;
        pthread_mutex_unlock(&sched_lock);
        sem_post(&yield_sem);
        sem_wait(&process_table[current_process].run_token);
    }
}

void mutex_release(Mutex *m) {
    pthread_mutex_lock(&m->internal);
    if (m->owner_id != current_process) { pthread_mutex_unlock(&m->internal); return; }
    m->is_locked = false;
    m->owner_id  = -1;
    pthread_mutex_unlock(&m->internal);
    pthread_mutex_lock(&sched_lock);
    process_table[current_process].priority    = process_table[current_process].base_priority;
    for (int i = 0; i < process_count; i++)
        if (process_table[i].state == BLOCKED && process_table[i].sleep_ticks == -1)
            process_table[i].state = READY;
    pthread_mutex_unlock(&sched_lock);
    os_yield();
}

/* ── Message Queue ───────────────────────────────────────────────────── */
void mq_init(MessageQueue *q) { q->head=q->tail=q->count=0; mutex_init(&q->lock); }

void mq_send(MessageQueue *q, int data) {
    while (1) {
        mutex_acquire(&q->lock);
        if (q->count < QUEUE_MAX_SIZE) {
            q->buffer[q->tail] = data;
            q->tail = (q->tail+1) % QUEUE_MAX_SIZE;
            q->count++;
            mutex_release(&q->lock);
            return;
        }
        mutex_release(&q->lock);
        os_yield();
    }
}

int mq_receive(MessageQueue *q) {
    while (1) {
        mutex_acquire(&q->lock);
        if (q->count > 0) {
            int data = q->buffer[q->head];
            q->head = (q->head+1) % QUEUE_MAX_SIZE;
            q->count--;
            mutex_release(&q->lock);
            return data;
        }
        mutex_release(&q->lock);
        os_delay(10);
    }
}

/* ── RTOS Semaphore ──────────────────────────────────────────────────── */
void rtos_sem_init(Semaphore *s, int max) { s->count=max; s->max_count=max; mutex_init(&s->lock); }
void rtos_sem_wait(Semaphore *s) {
    while (1) {
        mutex_acquire(&s->lock);
        if (s->count > 0) { s->count--; mutex_release(&s->lock); return; }
        mutex_release(&s->lock);
        os_delay(5);
    }
}
void rtos_sem_post(Semaphore *s) {
    mutex_acquire(&s->lock);
    if (s->count < s->max_count) s->count++;
    mutex_release(&s->lock);
}

/* ── EDF scheduler helper ────────────────────────────────────────────── */
static int pick_next_edf(long long now) {
    int    best = -1;
    long long earliest = LLONG_MAX;
    for (int i = 0; i < process_count; i++) {
        if (process_table[i].state != READY && process_table[i].state != RUNNING) continue;
        if (process_table[i].deadline_ms < earliest) {
            earliest = process_table[i].deadline_ms;
            best = i;
        }
    }
    return best;
}

static int pick_next_priority(void) {
    int best = -1, highest = -1;
    for (int i = 0; i < process_count; i++) {
        if ((process_table[i].state == READY || process_table[i].state == RUNNING) &&
            process_table[i].priority > highest) {
            highest = process_table[i].priority;
            best = i;
        }
    }
    return best;
}

/* ── Scheduler thread ────────────────────────────────────────────────── */
static void *scheduler_thread(void *arg) {
    (void)arg;
    struct timespec tick = { 0, TICK_NS };
    printf("Scheduler thread started (policy: %s, preemption: %dms)\n\n",
           sched_policy == SCHED_EDF ? "EDF" : "PRIORITY", PREEMPT_MS);

    while (1) {
        nanosleep(&tick, NULL);

        preempt_guard = 1;              /* block signal during switch   */
        pthread_mutex_lock(&sched_lock);

        long long now = now_ms();

        /* ── Tick sleep timers ─────────────────────────────────────── */
        for (int i = 0; i < process_count; i++) {
            if (process_table[i].state == BLOCKED && process_table[i].sleep_ticks > 0) {
                if (--process_table[i].sleep_ticks == 0)
                    process_table[i].state = READY;
            }
        }

        /* ── EDF: check for missed deadlines, renew periodic tasks ─── */
        if (sched_policy == SCHED_EDF) {
            for (int i = 0; i < process_count; i++) {
                if (process_table[i].state == TERMINATED) continue;
                if (process_table[i].deadline_ms != LLONG_MAX &&
                    now > process_table[i].deadline_ms &&
                    process_table[i].state != TERMINATED) {
                    process_table[i].missed_deadlines++;
                    printf("[EDF] Task %d MISSED DEADLINE (missed: %d)\n",
                           i, process_table[i].missed_deadlines);
                    /* Renew if periodic */
                    if (process_table[i].period_ms > 0)
                        process_table[i].deadline_ms += process_table[i].period_ms;
                }
            }
        }

        /* ── Pick next task ─────────────────────────────────────────── */
        int next = (sched_policy == SCHED_EDF)
                   ? pick_next_edf(now)
                   : pick_next_priority();

        /* ── Count active (non-terminated) tasks ───────────────────── */
        int active = 0;
        for (int i = 0; i < process_count; i++)
            if (process_table[i].state != TERMINATED) active++;

        if (active == 0) {
            pthread_mutex_unlock(&sched_lock);
            preempt_guard = 0;
            break;
        }

        if (next == -1) {
            pthread_mutex_unlock(&sched_lock);
            preempt_guard = 0;
            continue;
        }

        current_process           = next;
        process_table[next].state = RUNNING;
        pthread_mutex_unlock(&sched_lock);

        sem_post(&process_table[next].run_token);
        preempt_guard = 0;              /* signal now allowed again     */
        sem_wait(&yield_sem);           /* wait for yield or preemption */
    }
    return NULL;
}

/* ── os_start ────────────────────────────────────────────────────────── */
void os_start(void) {
    printf("Starting Threaded RTOS Kernel v2 (%d tasks)...\n", process_count);
    install_preemption_timer();

    pthread_t sched;
    pthread_create(&sched, NULL, scheduler_thread, NULL);
    pthread_join(sched, NULL);

    stop_preemption_timer();

    /* Print EDF stats */
    if (sched_policy == SCHED_EDF) {
        printf("\n── EDF Deadline Report ──────────────────────────────\n");
        for (int i = 0; i < process_count; i++)
            printf("  Task %d: %d missed deadline(s)\n",
                   i, process_table[i].missed_deadlines);
    }

    for (int i = 0; i < process_count; i++) {
        pthread_join(process_table[i].thread, NULL);
        sem_destroy(&process_table[i].run_token);
    }
    sem_destroy(&yield_sem);
    printf("All processes terminated. Shutting down OS.\n");
}
