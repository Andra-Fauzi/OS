#pragma once
#include "terminal.h"
#include "isr.h"
#include "vfs.h"

void sys_exit(struct interrupt_frame *frame);
void sys_read(struct interrupt_frame *frame);
void sys_write(struct interrupt_frame *frame);
void sys_open(struct interrupt_frame *frame);
void sys_close(struct interrupt_frame *frame);
void sys_lseek(struct interrupt_frame *frame);
void sys_stat(struct interrupt_frame *frame);
void sys_fstat(struct interrupt_frame *frame);
void sys_mmap(struct interrupt_frame *frame);
void sys_waitpid(struct interrupt_frame *frame);
void sys_execve(struct interrupt_frame *frame);
void sys_fork(struct interrupt_frame *frame);
void sys_dup(struct interrupt_frame *frame);
void sys_dup2(struct interrupt_frame *frame);
void sys_pipe(struct interrupt_frame *frame);
void sys_chdir(struct interrupt_frame *frame);
void sys_getcwd(struct interrupt_frame *frame);
void sys_sigaction(struct interrupt_frame *frame);
void sys_kill(struct interrupt_frame *frame);
void sys_nanosleep(struct interrupt_frame *frame);
void sys_clock_gettime(struct interrupt_frame *frame);
void sys_ioctl(struct interrupt_frame *frame);
void sys_fcntl(struct interrupt_frame *frame);