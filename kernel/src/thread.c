#include "thread.h"
#include "ehci.h"
#include "vfs.h"
thread_t *main_thread = NULL;
thread_t *running_thread = NULL;

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
}

void add_thread(thread_t *thread) {
    thread_t *ptr_thread = main_thread;
    while(ptr_thread->next != main_thread) {
        ptr_thread = ptr_thread->next;
    }
    ptr_thread->next = thread;
    thread->next = main_thread;
}

void create_thread(thread_t *thread, void (*func)()) {
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
    thread->frame.cs = 0x28;
    thread->frame.rflags = 0x282;
    // Increase stack size to 8KB
    thread->frame.rsp = (uint64_t)malloc(8192, 16) + 8192;
    thread->frame.ss = 0x30;
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