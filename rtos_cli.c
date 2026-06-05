/*
 * rtos_cli.c — Interactive CLI for the Threaded RTOS
 *
 * Features:
 *   - Run any of the four built-in test scenarios
 *   - Real-time task status monitor (printed every second while OS runs)
 *   - File sharing: save/load task logs to disk, list saved sessions
 *   - "snapshot" command: dump current PCB table to a timestamped file
 *
 * Build:
 *   gcc -Wall -o rtos_cli rtos_cli.c rtos_threaded.c \
 *       test_message_queue.c test_priority_inversion.c \
 *       test_readers_writers.c test_semaphore.c \
 *       -lpthread -Wno-unused-result
 *
 * Usage:
 *   ./rtos_cli                  (interactive menu)
 *   ./rtos_cli --run semaphore  (run directly from shell)
 *   ./rtos_cli --list           (list saved session files)
 *   ./rtos_cli --load session_20240101_120000.log (replay a log)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "rtos_threaded.h"

/* ---- Logging ----------------------------------------------------------- */
#define LOG_DIR  "./rtos_sessions"
#define LOG_MAX  8192

static FILE   *log_fp     = NULL;
static char    log_path[256];
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Intercept printf via a macro — redirect to both stdout and the log file */
static int rtos_log(const char *fmt, ...) {
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);

    if (log_fp) {
        pthread_mutex_lock(&log_mutex);
        va_start(ap, fmt);
        vfprintf(log_fp, fmt, ap);
        va_end(ap);
        fflush(log_fp);
        pthread_mutex_unlock(&log_mutex);
    }
    return n;
}

/* ---- Scenario forward declarations ------------------------------------ */
/* Each test file has a main(); we rename them here to avoid conflicts.
   The test files are #included (not linked separately) so we can rename. */

/* We include each test's logic by redefining main */
void run_semaphore_demo(void);
void run_message_queue_demo(void);
void run_priority_inversion_demo(void);
void run_readers_writers_demo(void);
void run_edf_demo(void);
void run_preemption_demo(void);

/* ---- File-sharing helpers --------------------------------------------- */

static void ensure_log_dir(void) {
    struct stat st = {0};
    if (stat(LOG_DIR, &st) == -1) {
        if (mkdir(LOG_DIR, 0755) != 0 && errno != EEXIST) {
            perror("mkdir " LOG_DIR);
        }
    }
}

static void open_log(const char *scenario) {
    ensure_log_dir();
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(log_path, sizeof(log_path),
             LOG_DIR "/%s_%04d%02d%02d_%02d%02d%02d.log",
             scenario,
             t->tm_year+1900, t->tm_mon+1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    log_fp = fopen(log_path, "w");
    if (!log_fp) perror("fopen log");
}

static void close_log(void) {
    if (log_fp) { fclose(log_fp); log_fp = NULL; }
}

static void list_sessions(void) {
    DIR *d = opendir(LOG_DIR);
    if (!d) { printf("No saved sessions in %s\n", LOG_DIR); return; }

    printf("\n  Saved sessions in %s/\n", LOG_DIR);
    printf("  %-50s  %s\n", "File", "Size");
    printf("  %s\n", "────────────────────────────────────────────────────");

    struct dirent *de;
    int count = 0;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char fullpath[300];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", LOG_DIR, de->d_name);
        struct stat st;
        stat(fullpath, &st);
        printf("  %-50s  %lld bytes\n", de->d_name, (long long)st.st_size);
        count++;
    }
    closedir(d);
    if (count == 0) printf("  (none yet)\n");
    printf("\n");
}

static void replay_session(const char *filename) {
    char fullpath[300];
    if (filename[0] == '/') {
        strncpy(fullpath, filename, sizeof(fullpath)-1);
    } else {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", LOG_DIR, filename);
    }
    FILE *f = fopen(fullpath, "r");
    if (!f) { perror("fopen"); return; }
    printf("\n─── Replaying: %s ────────────────────────────\n\n", filename);
    char line[512];
    while (fgets(line, sizeof(line), f))
        fputs(line, stdout);
    fclose(f);
    printf("\n─── End of replay ──────────────────────────────────────────\n\n");
}

/* ---- PCB Status Monitor ------------------------------------------------ */
/*
 * Runs in a separate thread while the RTOS is active.
 * Prints a compact status table every STATUS_INTERVAL_MS milliseconds.
 */
#define STATUS_INTERVAL_MS 500
static volatile int monitor_running = 0;

static const char *state_str(ProcessState s) {
    switch (s) {
        case READY:      return "READY    ";
        case RUNNING:    return "RUNNING  ";
        case BLOCKED:    return "BLOCKED  ";
        case TERMINATED: return "TERMINATED";
        default:         return "UNKNOWN  ";
    }
}

static void *monitor_thread(void *arg) {
    (void)arg;
    struct timespec ts = { 0, STATUS_INTERVAL_MS * 1000000L };
    while (monitor_running) {
        nanosleep(&ts, NULL);
        if (!monitor_running) break;

        printf("\n  ┌─ PCB Snapshot ──────────────────────────────────────────┐\n");
        printf("  │ %-4s %-8s %-10s %-10s %-10s │\n",
               "ID", "Prio", "BasePrio", "State", "SleepTick");
        printf("  ├──────────────────────────────────────────────────────────┤\n");
        for (int i = 0; i < process_count; i++) {
            printf("  │ %-4d %-8d %-10d %-10s %-10d │\n",
                   process_table[i].id,
                   process_table[i].priority,
                   process_table[i].base_priority,
                   state_str(process_table[i].state),
                   process_table[i].sleep_ticks);
        }
        printf("  └──────────────────────────────────────────────────────────┘\n\n");
    }
    return NULL;
}

static pthread_t monitor_tid;

static void start_monitor(void) {
    monitor_running = 1;
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);
}
static void stop_monitor(void) {
    monitor_running = 0;
    pthread_join(monitor_tid, NULL);
}

/* ---- Snapshot ---------------------------------------------------------- */
static void snapshot_pcb(void) {
    ensure_log_dir();
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char snap[256];
    snprintf(snap, sizeof(snap),
             LOG_DIR "/snapshot_%04d%02d%02d_%02d%02d%02d.txt",
             t->tm_year+1900, t->tm_mon+1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    FILE *f = fopen(snap, "w");
    if (!f) { perror("snapshot"); return; }
    fprintf(f, "PCB Snapshot — %s\n", ctime(&now));
    fprintf(f, "%-4s %-8s %-10s %-10s %-10s\n",
            "ID","Prio","BasePrio","State","SleepTick");
    for (int i = 0; i < process_count; i++) {
        fprintf(f, "%-4d %-8d %-10d %-10s %-10d\n",
                process_table[i].id,
                process_table[i].priority,
                process_table[i].base_priority,
                state_str(process_table[i].state),
                process_table[i].sleep_ticks);
    }
    fclose(f);
    printf("  Snapshot saved → %s\n", snap);
}

/* ---- Demo implementations --------------------------------------------- */

/* Each demo re-inits the OS and runs the scenario inline */

/* Forward declarations from test files */
extern MessageQueue sensor_queue;
extern void producer_task(void);
extern void consumer_task(void);

extern Mutex shared_resource;
extern void task_low(void);
extern void task_medium(void);
extern void task_high(void);

extern Semaphore resource_lock;
extern Mutex rmutex;
extern int read_count;
extern void reader_task(void);
extern void writer_task(void);

extern Semaphore parking_lot;
extern void car_task(void);

void run_message_queue_demo(void) {
    os_init(); mq_init(&sensor_queue);
    create_process(producer_task, 2);
    create_process(consumer_task, 4);
    os_start();
}

void run_priority_inversion_demo(void) {
    os_init(); mutex_init(&shared_resource);
    create_process(task_low,    1);
    create_process(task_medium, 3);
    create_process(task_high,   5);
    os_start();
}

void run_readers_writers_demo(void) {
    os_init();
    sem_init(&resource_lock, 1);
    mutex_init(&rmutex);
    read_count = 0;
    create_process(reader_task, 2);
    create_process(reader_task, 2);
    create_process(reader_task, 2);
    create_process(writer_task, 2);
    create_process(writer_task, 2);
    os_start();
}

void run_semaphore_demo(void) {
    os_init();
    sem_init(&parking_lot, 3);
    create_process(car_task, 2);
    create_process(car_task, 2);
    create_process(car_task, 2);
    create_process(car_task, 2);
    create_process(car_task, 2);
    os_start();
}

/* ---- Scenario table --------------------------------------------------- */
typedef struct {
    const char *key;
    const char *name;
    const char *description;
    void (*run)(void);
} Scenario;

static Scenario scenarios[] = {
    { "semaphore",  "Semaphore Demo",
      "5 cars compete for 3 parking spots (counting semaphore)",
      run_semaphore_demo },
    { "queue",      "Message Queue Demo",
      "Producer/Consumer with priority-driven scheduling",
      run_message_queue_demo },
    { "inversion",  "Priority Inversion Demo",
      "Low→Medium→High tasks + priority inheritance on mutex",
      run_priority_inversion_demo },
    { "readers",    "Readers-Writers Demo",
      "3 readers + 2 writers sharing a database semaphore",
      run_readers_writers_demo },
    { "edf", "EDF Scheduling Demo",
     "Earliest Deadline First scheduler",
    run_edf_demo },
    { "preemption", "Preemption Demo",
     "SIGALRM-based preemption",
    run_preemption_demo },
};
#define N_SCENARIOS ((int)(sizeof(scenarios)/sizeof(scenarios[0])))

/* ---- CLI --------------------------------------------------------------- */

static void print_banner(void) {
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════════════╗\n");
    printf("  ║         Threaded RTOS — Interactive CLI Shell            ║\n");
    printf("  ║         (POSIX threads, no Windows dependency)           ║\n");
    printf("  ╚══════════════════════════════════════════════════════════╝\n\n");
}

static void print_menu(void) {
    printf("  Scenarios:\n");
    for (int i = 0; i < N_SCENARIOS; i++) {
        printf("    %d) %-28s — %s\n",
               i+1, scenarios[i].name, scenarios[i].description);
    }
    printf("\n");
    printf("  Utilities:\n");
    printf("    l) List saved session logs\n");
    printf("    r) Replay a saved session\n");
    printf("    s) Snapshot current PCB table\n");
    printf("    q) Quit\n\n");
    printf("  > ");
}

static void run_scenario(Scenario *sc, bool with_monitor, bool save_log) {
    printf("\n═══ Running: %s ════════════════════════\n\n", sc->name);

    if (save_log) open_log(sc->key);
    if (with_monitor) start_monitor();

    sc->run();

    if (with_monitor) stop_monitor();
    if (save_log) {
        close_log();
        printf("\n  Session log saved → %s\n", log_path);
    }
    printf("\n═══ Done ══════════════════════════════════════════════════\n\n");
}

static Scenario *find_scenario(const char *key) {
    for (int i = 0; i < N_SCENARIOS; i++)
        if (strcmp(scenarios[i].key, key) == 0)
            return &scenarios[i];
    return NULL;
}

int main(int argc, char *argv[]) {
    /* ---- Command-line mode ---------------------------------------- */
    if (argc > 1) {
        if (strcmp(argv[1], "--list") == 0) {
            list_sessions(); return 0;
        }
        if (strcmp(argv[1], "--load") == 0 && argc > 2) {
            replay_session(argv[2]); return 0;
        }
        if (strcmp(argv[1], "--run") == 0 && argc > 2) {
            Scenario *sc = find_scenario(argv[2]);
            if (!sc) { fprintf(stderr, "Unknown scenario: %s\n", argv[2]); return 1; }
            run_scenario(sc, /*monitor=*/false, /*log=*/true);
            return 0;
        }
        fprintf(stderr,
            "Usage:\n"
            "  %s                        interactive menu\n"
            "  %s --run <key>            run scenario (keys: semaphore queue inversion readers)\n"
            "  %s --list                 list saved sessions\n"
            "  %s --load <file>          replay a session log\n",
            argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }

    /* ---- Interactive menu ----------------------------------------- */
    print_banner();
    char input[128];

    while (1) {
        print_menu();
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = '\0';

        if (input[0] == 'q' || input[0] == 'Q') {
            printf("  Goodbye.\n\n"); break;
        }
        if (input[0] == 'l') { list_sessions(); continue; }
        if (input[0] == 's') { snapshot_pcb(); continue; }
        if (input[0] == 'r') {
            printf("  Enter filename (or path): ");
            if (!fgets(input, sizeof(input), stdin)) continue;
            input[strcspn(input, "\n")] = '\0';
            replay_session(input); continue;
        }

        int choice = atoi(input);
        if (choice >= 1 && choice <= N_SCENARIOS) {
            Scenario *sc = &scenarios[choice-1];

            /* Ask about monitor */
            printf("  Show live PCB monitor? [y/N]: ");
            char yn[8] = {0};
            if (!fgets(yn, sizeof(yn), stdin)) yn[0] = 'n';
            bool monitor = (yn[0] == 'y' || yn[0] == 'Y');

            /* Ask about logging */
            printf("  Save session log? [Y/n]: ");
            char yn2[8] = {0};
            if (!fgets(yn2, sizeof(yn2), stdin)) yn2[0] = 'y';
            bool save = !(yn2[0] == 'n' || yn2[0] == 'N');

            run_scenario(sc, monitor, save);
        } else {
            printf("  Invalid choice. Try again.\n\n");
        }
    }
    return 0;
}

/* ---- EDF and Preemption demos (appended for v2) ----------------------- */
extern void task_edf_a(void);
extern void task_edf_b(void);
extern void task_edf_c(void);
extern void task_low_busy(void);
extern void task_high_preempt(void);
extern void task_med_watcher(void);

void run_edf_demo(void) {
    os_init();
    os_set_policy(SCHED_EDF);
    create_process_edf(task_edf_a, 180, 200);
    create_process_edf(task_edf_b, 450, 500);
    create_process_edf(task_edf_c, 280, 300);
    os_start();
}

void run_preemption_demo(void) {
    os_init();
    create_process(task_low_busy,    1);
    create_process(task_med_watcher, 3);
    create_process(task_high_preempt,5);
    os_start();
}
