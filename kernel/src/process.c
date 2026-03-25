#include "process.h"
#include "ehci.h"
#include "vfs.h"

process_t *main_process = NULL;
process_t *running_process = NULL;
process_t *process_garbage = NULL;

thread_t *main_thread = NULL;
thread_t *running_thread = NULL;
thread_t *thread_garbage = NULL;

uint32_t PID_TOTAL = 0;

void lock_process() {
    asm volatile("cli");
}

void unlock_process() {
    asm volatile("sti");
}

static void enable_sse() {
    lock_process();
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((uint64_t)1 << 2); // clear EM
    cr0 |= (1 << 1);  // set MP
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // set OSFXSR
    cr4 |= (1 << 10); // set OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
    unlock_process();
}

static uint8_t initial_fpu_state[512] __attribute__((aligned(16)));

static void init_fpu_state_buffer() {
    lock_process();
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : : "m"(initial_fpu_state));
    unlock_process();
}

void init_thread() {
    lock_process();
    enable_sse();
    init_fpu_state_buffer();
    main_process = (process_t *)malloc(sizeof(process_t), 16);
    main_process->pid = PID_TOTAL;
    PID_TOTAL++;
    main_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    memcpy(main_thread->fpu_state, initial_fpu_state, 512);
    // thread_t *first_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    // create_thread(first_thread, first);
    // thread_t *second_thread = (thread_t *)malloc(sizeof(thread_t), 16);
    // create_thread(second_thread, second);
    // main_thread->next = first_thread;
    // first_thread->next = second_thread;
    // second_thread->next = main_thread;
    main_thread->next = main_thread;
    running_thread = main_thread;
    main_thread->pid = PID_TOTAL;
    PID_TOTAL++;
    main_process->threads = main_thread;
    main_process->next = main_process;
    running_process = main_process;
    unlock_process();
}

void add_process(process_t *process) {
    lock_process();
    process_t *ptr_process = main_process;
    do {
        if(ptr_process->next == main_process) {
            break;
        }
        ptr_process = ptr_process->next;
    } while(ptr_process != main_process);
    process->pid = PID_TOTAL;
    ptr_process->next = process;
    process->next = main_process;
    PID_TOTAL++;
    unlock_process();
}

void add_thread_to_pid(thread_t *thread, uint32_t pid_process) {
    lock_process();
    process_t *ptr_process = main_process;
    while(ptr_process->next != main_process) {
        ptr_process = ptr_process->next;
        if(ptr_process->pid == pid_process) {
            break;
        }
    }
    if(ptr_process->pid != pid_process) {
        printf("Process not found\n");
        unlock_process();
        return;
    }
    thread_t *ptr_thread = ptr_process->threads;
    while(ptr_thread->next != ptr_process->threads) {
        ptr_thread = ptr_thread->next;
    }
    thread->pid = PID_TOTAL;
    ptr_thread->next = thread;
    thread->next = ptr_process->threads;
    PID_TOTAL++;
    unlock_process();
}

void add_thread(thread_t *thread, process_t *process) {
    lock_process();
    if(process->threads == NULL) {
        process->threads = thread;
        thread->next = thread;
        thread->pid = PID_TOTAL;
        PID_TOTAL++;
        unlock_process();
        return;
    }
    thread_t *ptr_thread = process->threads;
    while(ptr_thread->next != process->threads) {
        ptr_thread = ptr_thread->next;
    }
    thread->pid = PID_TOTAL;
    ptr_thread->next = thread;
    thread->next = process->threads;
    PID_TOTAL++;
    unlock_process();
}

void create_process(process_t *process, void(*func)()) {
    lock_process();
    process->pid = PID_TOTAL;
    PID_TOTAL++;
    process->threads = NULL;
    process->next = NULL;
    thread_t *thread = (thread_t *)malloc(sizeof(thread_t), 16);
    thread->next = thread;
    create_thread(thread, func);
    add_thread(thread, process);
    unlock_process();
}

void create_thread(thread_t *thread, void (*func)()) {
    lock_process();
    thread->pid = 0;
    thread->frame.r15 = 0;
    thread->frame.r14 = 0;
    thread->frame.r13 = 0;
    thread->frame.r12 = 0;
    thread->frame.r11 = 0;
    thread->frame.r10 = 0;
    thread->frame.r9 = 0;
    thread->frame.r8 = 0;
    thread->frame.rsi = 0;
    thread->frame.rdi = 0;
    thread->frame.rbp = 0;
    thread->frame.rdx = 0;
    thread->frame.rcx = 0;
    thread->frame.rbx = 0;
    thread->frame.rax = 0;
    thread->frame.rip = (uint64_t)func;
    thread->frame.cs = 0x08;
    thread->frame.rflags = 0x202;
    // Increase stack size to 8KB
    thread->stack_base = malloc(8192, 16);
    thread->frame.rsp = (uint64_t)thread->stack_base + 8192;
    thread->frame.ss = 0x10;
    thread->lock = false;
    memcpy(thread->fpu_state, initial_fpu_state, 512);
    unlock_process();
}

void switch_context(struct interrupt_frame *frame) {
    lock_process();

    if(thread_garbage != NULL) {
        free(thread_garbage->stack_base);
        free(thread_garbage);
        thread_garbage = NULL;
    }

    if(process_garbage != NULL) {
        thread_t *ptr_thread = process_garbage->threads;
        while(ptr_thread->next != process_garbage->threads) {
            thread_t *temp = ptr_thread;
            ptr_thread = ptr_thread->next;
            free(temp->stack_base);
            free(temp);
        }
        free(ptr_thread->stack_base);
        free(ptr_thread);
        free(process_garbage);
        process_garbage = NULL;
    }

    if (!running_thread || running_thread->lock) {
        unlock_process();
        return;
    }

    // Save current thread
    __asm__ volatile("fxsave %0" : : "m"(running_thread->fpu_state));
    running_thread->frame = *frame;

    // Move to next thread
    thread_t *start = running_process->threads;
    running_thread = running_thread->next;

    // If we looped back → switch process
    if (running_thread == start) {
        running_process = running_process->next;
        running_thread = running_process->threads;
    }

    // Load next thread
    *frame = running_thread->frame;
    __asm__ volatile("fxrstor %0" : : "m"(running_thread->fpu_state));

    unlock_process();
}

void remove_thread() {
    lock_process();
    if (running_thread == NULL) {
        unlock_process();
        return;
    }
    if (running_thread->pid == 0) {
        unlock_process();
        return;
    }
    if (running_thread->lock) {
        unlock_process();
        return;
    }
    
    /* Find predecessor of the running thread in the circular list. */
    thread_t *prev = main_thread;
    if (prev == NULL) {
        unlock_process();
        return;
    }
    
    while (prev->next != running_thread) {
        prev = prev->next;
        if (prev == main_thread) {
            /* running_thread not found in list; nothing to do */
            unlock_process();
            return;
        }
    }
    
    /* Unlink the running thread from the circular list.
    Do NOT free the thread structure here: the thread is still running
    on its stack and its state will be referenced by the next interrupt
    / scheduler. Freeing now would lead to use-after-free. */
    prev->next = running_thread->next;
    thread_garbage = running_thread;
    
    unlock_process();
}

void remove_process() {
    lock_process();
    if (running_process == NULL) {
        unlock_process();
        return;
    }
    if (running_process->pid == 0) {
        unlock_process();
        return;
    }
    
    /* Find predecessor of the running thread in the circular list. */
    process_t *prev = main_process;
    if (prev == NULL) {
        unlock_process();
        return;
    }
    
    while (prev->next != running_process) {
        prev = prev->next;
        if (prev == main_process) {
            /* running_process not found in list; nothing to do */
            unlock_process();
            return;
        }
    }
    
    /* Unlink the running thread from the circular list.
    Do NOT free the thread structure here: the thread is still running
    on its stack and its state will be referenced by the next interrupt
    / scheduler. Freeing now would lead to use-after-free. */
    prev->next = running_process->next;
    process_garbage = running_process;
    
    unlock_process();
}

void kill_running_thread(struct interrupt_frame *frame) {
    asm volatile("cli");
    if (running_thread == NULL) {
        asm volatile("sti");
        return;
    }
    /* Do not kill the main thread (pid 0) */
    if (running_thread->pid == 0) {
        printf("this thread is main thread\n");
        asm volatile("sti");
        return;
    }
    if (running_thread->lock) {
        asm volatile("sti");
        return;
    }

    /* Save FPU state and register state of the current thread */
    __asm__ volatile("fxsave %0" : : "m"(running_thread->fpu_state));
    running_thread->frame = *frame;

    /* Find predecessor in circular list */
    thread_t *prev = main_thread;
    if (prev == NULL) {
        asm volatile("sti");
        return;
    }

    while (prev->next != running_thread) {
        prev = prev->next;
        if (prev == main_thread) {
            /* running_thread not found */
            asm volatile("sti");
            return;
        }
    }

    thread_t *to_free = running_thread;
    prev->next = to_free->next;

    /* Switch running_thread to the next thread and load its state into the
       interrupt frame so the CPU resumes on that thread when the interrupt
       returns. */
    running_thread = to_free->next;
    *frame = running_thread->frame;
    __asm__ volatile("fxrstor %0" : : "m"(running_thread->fpu_state));

    /* Now safe to free the old thread structure */
    free(to_free);

    asm volatile("sti");
}

void total_process() {
    lock_process();
    process_t *prev = main_process;
    if (prev == NULL) {
        return;
    }

    int total = 1;

    while (prev->next != main_process) {
        prev = prev->next;
        total++;
    }

    printf("total process is %d\n", total);
    unlock_process();
}

void total_thread() {
    lock_process();
    thread_t *prev = main_thread;
    if (prev == NULL) {
        return;
    }

    int total = 1;

    while (prev->next != main_thread) {
        prev = prev->next;
        total++;
    }

    printf("total thread is %d\n", total);
    unlock_process();
}