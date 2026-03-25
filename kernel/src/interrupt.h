#pragma once
#include "apic.h"
#include "isr.h"
#include "process.h"
#include "idt.h"
#include "terminal.h"
#include "syscall.h"

void init_interrupt();

extern void timer_stub();
extern void syscall_stub();
