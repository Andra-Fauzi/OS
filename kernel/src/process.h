#pragma once
#include <stdint.h>
#include "isr.h"
#include "vfs.h"

typedef struct process {
    uint32_t pid;

    char cwd[256];
    vfs_file_t open_files[MAX_OPEN_FILES];
    struct thread *threads;
    struct process *next;
    bool free;
} process_t;

typedef struct thread {
    struct interrupt_frame frame;
    struct thread *next;
    bool lock;
    uint8_t fpu_state[512] __attribute__((aligned(16)));
    uint32_t tid;
    void *stack_base;
    bool free;
} thread_t;

void total_process();
void total_thread();
void list_process();
void list_thread();
void create_process(process_t *process, void(*func)());
void create_thread(thread_t *thread, void (*func)());
void init_thread();
void init_process_stdio();
void switch_context(struct interrupt_frame *frame);
void kill_running_thread(struct interrupt_frame *frame);
void add_thread_to_pid(thread_t *thread, uint32_t pid_process);
void add_process(process_t *process);
void remove_process();
void remove_thread();
void add_thread(thread_t *thread, process_t *process);

/*
+------------------------------------------------------+
|                     Process                          |
|------------------------------------------------------|
| Memory Space:                                        |
|   - Code Segment (shared by all threads)            |
|   - Data Segment / Heap (shared)                    |
|   - Global Variables (shared)                       |
|                                                      |
| File Descriptors Table (shared)                      |
|   FD 0 -> stdin                                      |
|   FD 1 -> stdout                                     |
|   FD 2 -> stderr                                     |
|   FD 3 -> open file                                  |
|                                                      |
| Current Working Directory (cwd)                      |
|                                                      |
| Threads:                                            |
|   +------------------+   +------------------+       |
|   | Thread 1          |   | Thread 2         |       |
|   | Stack (private)   |   | Stack (private)  |       |
|   | CPU Registers     |   | CPU Registers    |       |
|   +------------------+   +------------------+       |
|                                                      |
|   +------------------+                               |
|   | Thread 3          |                               |
|   | Stack (private)   |                               |
|   | CPU Registers     |                               |
|   +------------------+                               |
+------------------------------------------------------+

File Descriptors Table plan:

We will build a table of file descriptors for each process.
first 3 file descriptors are reserved for stdin, stdout, and stderr.
how it work? first FD 0 its file_descriptors[0] but the value is another fd.
file_descriptors[0] is 5, then we use the value to the real vfs because we not gonna overwrite
the real vfs. but other than 3 first file descriptors, we will use the value to the real vfs.

Diagram:

+------------------------------------------------------+
|                     Process                          |
|------------------------------------------------------|
| File Descriptors Table:                              |
|   FD 0 -> 5                                          |
|   FD 1 -> 6                                          |
|   FD 2 -> 7                                          |
|   FD 3 -> 8                                          |
|   FD 4 -> 9                                          |
|   ...                                                |
+------------------------------------------------------+

+------------------------------------------------------+
|                     VFS                              |
|------------------------------------------------------|
| File Descriptors Table:                              |
|   FD 0 -> stdin                                      |
|   FD 1 -> stdout                                     |
|   FD 2 -> stderr                                     |
|   FD 3 -> open file                                  |
|   FD 4 -> open file                                  |
|   ...                                                |
+------------------------------------------------------+

Thread Plan:

We will build a table of threads for each process.
so after we create a process, we will create a thread for that process.
and the thread will be the main thread of that process.
and switch_context will switch between threads of the same process, until it got to main thread.
then it will switch to next process.


*/