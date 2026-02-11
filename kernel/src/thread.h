#pragma once
#include <stdint.h>
#include "isr.h"

typedef struct thread {
    struct interrupt_frame frame;
    struct thread *next;
    bool lock;
    uint8_t fpu_state[512] __attribute__((aligned(16)));
} thread_t;


void create_thread(thread_t *thread, void (*func)());
void init_thread();
void switch_thread(struct interrupt_frame *frame);