#pragma once
#include "apic.h"
#include "isr.h"
#include "thread.h"
#include "idt.h"

void init_interrupt();

extern void timer_stub();