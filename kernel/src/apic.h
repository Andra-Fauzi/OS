#pragma once
#include <stdint.h>
#include "terminal.h"
#include "paging.h"

void pic_disable();
void lapic_enable();
__attribute__((interrupt))
void isr_timer(void* frame);
void lapic_timer_init();
__attribute__((interrupt))
void isr_spurious(void* frame);
__attribute__((interrupt))
void isr_default(void *frame);
__attribute__((interrupt))
void isr_default_err(void *frame, uint64_t error);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t val);
void ioapic_enable_keyboard();
void sleep(uint64_t ms);