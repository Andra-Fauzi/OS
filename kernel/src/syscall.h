#pragma once
#include "terminal.h"
#include "isr.h"
#include "vfs.h"

void sys_exit(struct interrupt_frame *frame);
void sys_read(struct interrupt_frame *frame);
void sys_write(struct interrupt_frame *frame);