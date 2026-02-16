#include "thread.h"
#include "ehci.h"
#include "vfs.h"
thread_t *main_thread = NULL;
thread_t *running_thread = NULL;

uint32_t PID_TOTAL = 0;

static void enable_sse() {
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~((uint64_t)1 << 2); // clear EM
    cr0 |= (1 << 1);  // set MP
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // set OSFXSR
    cr4 |= (1 << 10); // set OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}

static uint8_t initial_fpu_state[512] __attribute__((aligned(16)));

static void init_fpu_state_buffer() {
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : : "m"(initial_fpu_state));
}

void init_thread() {
    enable_sse();
    init_fpu_state_buffer();
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
}

void add_thread(thread_t *thread) {
    thread_t *ptr_thread = main_thread;
    while(ptr_thread->next != main_thread) {
        ptr_thread = ptr_thread->next;
    }
    thread->pid = PID_TOTAL;
    ptr_thread->next = thread;
    thread->next = main_thread;
    PID_TOTAL++;
}

void create_thread(thread_t *thread, void (*func)()) {
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
    thread->frame.rsp = (uint64_t)malloc(8192, 16) + 8192;
    thread->frame.ss = 0x10;
    thread->lock = false;
    memcpy(thread->fpu_state, initial_fpu_state, 512);
}

void switch_thread(struct interrupt_frame *frame) {
    if (running_thread == NULL) {
        return;
    }
    if(running_thread->lock) {
        return;
    }
    __asm__ volatile("fxsave %0" : : "m"(running_thread->fpu_state));
    running_thread->frame = *frame;
    running_thread = running_thread->next;
    *frame = running_thread->frame;
    __asm__ volatile("fxrstor %0" : : "m"(running_thread->fpu_state));
}

void remove_thread() {
    asm volatile("cli");
    if (running_thread == NULL) {
        asm volatile("sti");
        return;
    }
    if (running_thread->pid == 0) {
        asm volatile("sti");
        return;
    }
    if (running_thread->lock) {
        asm volatile("sti");
        return;
    }

    /* Find predecessor of the running thread in the circular list. */
    thread_t *prev = main_thread;
    if (prev == NULL) {
        asm volatile("sti");
        return;
    }

    while (prev->next != running_thread) {
        prev = prev->next;
        if (prev == main_thread) {
            /* running_thread not found in list; nothing to do */
            asm volatile("sti");
            return;
        }
    }

    /* Unlink the running thread from the circular list.
       Do NOT free the thread structure here: the thread is still running
       on its stack and its state will be referenced by the next interrupt
       / scheduler. Freeing now would lead to use-after-free. */
    prev->next = running_thread->next;

    asm volatile("sti");
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

void total_thread() {
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

}