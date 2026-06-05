# Lightweight RTOS Kernel with Preemptive Scheduling

A lightweight Real-Time Operating System (RTOS) kernel implemented in C using POSIX threads. This project demonstrates the core concepts of real-time scheduling, synchronization primitives, inter-task communication, and resource management through a modular kernel architecture and a collection of focused test suites.

## Overview

This project simulates the behavior of a preemptive RTOS by providing:

* Priority-based task scheduling
* Preemptive context switching
* Earliest Deadline First (EDF) scheduling support
* Semaphores for task synchronization
* Message queues for inter-task communication
* Readers-Writers synchronization
* Priority inversion handling mechanisms
* Interactive command-line interface (CLI)
* Modular testing framework

The implementation is designed for educational purposes and demonstrates many of the fundamental concepts found in embedded RTOS kernels such as FreeRTOS, Zephyr, VxWorks, and RTEMS.

---

## Features

### Task Management

* Creation and deletion of tasks
* Task states management
* Priority assignment
* Preemptive scheduling
* Scheduler control mechanisms

### Real-Time Scheduling

#### Fixed Priority Scheduling

Tasks are selected according to their priority level.

* Higher priority tasks preempt lower priority tasks
* Deterministic scheduling behavior
* Suitable for real-time applications

#### Earliest Deadline First (EDF)

Dynamic scheduling algorithm where:

* Each task is assigned a deadline
* Scheduler selects the task with the nearest deadline
* Demonstrates advanced real-time scheduling techniques

### Synchronization Primitives

#### Semaphores

Supports:

* Binary semaphores
* Counting semaphores
* Mutual exclusion patterns
* Resource protection

#### Readers-Writers Synchronization

Allows:

* Concurrent readers
* Exclusive writers
* Protection of shared resources

### Inter-Task Communication

#### Message Queues

Tasks can:

* Send messages
* Receive messages
* Exchange data safely
* Decouple producers and consumers

### Priority Inversion Handling

Includes demonstrations of:

* Priority inversion scenarios
* Effects on real-time performance
* Resource contention analysis

---

## Project Structure

```text
.
├── rtos_threaded.c              # Core RTOS kernel implementation
├── rtos_cli.c                   # Interactive command-line interface
│
├── test_preemption.c            # Preemptive scheduling tests
├── test_edf.c                   # Earliest Deadline First scheduling tests
├── test_semaphore.c             # Semaphore functionality tests
├── test_message_queue.c         # Message queue tests
├── test_readers_writers.c       # Readers-Writers synchronization tests
├── test_priority_inversion.c    # Priority inversion demonstrations
│
├── rtos_sessions/               # CLI session data and logs
└── .git/
```

---

## Architecture

### Scheduler

The scheduler maintains a set of runnable tasks and determines which task executes next according to:

1. Task priority
2. Deadline information (EDF mode)
3. Task state transitions
4. Synchronization events

### Task Lifecycle

```text
Created
   │
   ▼
 Ready
   │
   ▼
Running
   │
 ┌─┴──────────────┐
 ▼               ▼
Blocked       Finished
 │
 ▼
Ready
```

### Kernel Components

```text
+-----------------------+
|     CLI Interface     |
+-----------+-----------+
            |
            v
+-----------------------+
|      RTOS Kernel      |
+-----------+-----------+
            |
  +---------+---------+
  |         |         |
  v         v         v
Tasks   Scheduler  IPC/Sync
                    |
      +------+------+------+
      |             |      |
      v             v      v
 Semaphores   Msg Queue   RW Lock
```

---

## Build Instructions

### Prerequisites

Ubuntu / WSL:

```bash
sudo apt update
sudo apt install build-essential
```

Verify installation:

```bash
gcc --version
```

### Compilation

```bash
gcc -Wall -O2 \
-D_POSIX_C_SOURCE=200809L \
-DCLI_BUILD \
rtos_threaded.c \
rtos_cli.c \
test_message_queue.c \
test_priority_inversion.c \
test_readers_writers.c \
test_semaphore.c \
test_edf.c \
test_preemption.c \
-o rtos_cli \
-lpthread
```

### Run

```bash
./rtos_cli
```

---

## Test Modules

### Preemption Test

Validates:

* Priority-based execution
* Scheduler responsiveness
* Context switching behavior

```text
test_preemption.c
```

---

### EDF Scheduling Test

Validates:

* Deadline assignment
* Dynamic priority changes
* Earliest deadline selection

```text
test_edf.c
```

---

### Semaphore Test

Validates:

* Resource locking
* Task synchronization
* Semaphore signaling

```text
test_semaphore.c
```

---

### Message Queue Test

Validates:

* Message transmission
* Queue ordering
* Producer-consumer workflows

```text
test_message_queue.c
```

---

### Readers-Writers Test

Validates:

* Shared resource access
* Reader concurrency
* Writer exclusivity

```text
test_readers_writers.c
```

---

### Priority Inversion Test

Demonstrates:

* Resource contention
* Blocking of high-priority tasks
* Real-time scheduling challenges

```text
test_priority_inversion.c
```

---

## Example Applications

This project can be used to study:

* Embedded systems scheduling
* Real-time operating systems
* Concurrent programming
* POSIX threads
* Synchronization primitives
* Resource allocation
* Deadline-based scheduling

---

## Learning Outcomes

By studying this project, users can understand:

* How RTOS schedulers work
* Why preemption is important
* How semaphores protect shared resources
* How message queues enable communication
* What causes priority inversion
* How EDF scheduling differs from fixed-priority scheduling
* Design trade-offs in real-time systems

---

## Future Enhancements

Potential improvements include:

* Priority inheritance protocol
* Rate Monotonic Scheduling (RMS)
* Tickless scheduler
* Memory pool management
* Dynamic task creation
* Event groups
* Software timers
* Multi-core scheduling support
* Performance profiling tools
* Real context switching implementation

---

## License

This project is intended for educational and research purposes.

