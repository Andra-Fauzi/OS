#include "interrupt.h"

#define LAPIC_EOI      0xB0

static volatile uint64_t tick = 0;

void sleep(uint64_t ms) {
	uint64_t target = tick + ms;
	while (tick < target) {
		asm volatile("hlt");
	}
}

void syscall_handler(struct interrupt_frame *frame) {
    switch(frame->rax) {
        case 60:
            sys_exit(frame);
            return;
        case 0:
            sys_read(frame);
            return;
        case 1:
            sys_write(frame);
            return;
        case 2:
            sys_open(frame);
            return;
        case 3:
            sys_close(frame);
            return;
        case 4:
            sys_stat(frame);
            return;
        case 5:
            sys_fstat(frame);
            return;
        case 8:
            sys_lseek(frame);
            return;
        case 7:
            sys_waitpid(frame);
            return;
        case 57:
            sys_fork(frame);
            return;
        case 59:
            sys_execve(frame);
            return;
        case 9:
            sys_mmap(frame);
            return;
        case 32:
            sys_dup(frame);
            return;
        case 33:
            sys_dup2(frame);
            return;
        case 22:
            sys_pipe(frame);
            return;
        case 80:
            sys_chdir(frame);
            return;
        case 183:
            sys_getcwd(frame);
            return;
        case 67:
            sys_sigaction(frame);
            return;
        case 62:
            sys_kill(frame);
            return;
        case 35:
            sys_nanosleep(frame);
            return;
        case 228:
            sys_clock_gettime(frame);
            return;
        case 16:
            sys_ioctl(frame);
            return;
        case 72:
            sys_fcntl(frame);
            return;
    }
}

void isr_timer_modified(struct interrupt_frame *frame) {
    tick++;
    lapic_write(LAPIC_EOI, 0);
    // printf("berubah\n");
    switch_context(frame);
}

void init_interrupt() {
    // 64 is same as 0x40
    set_idt_entry(0x40, timer_stub, 0x08, 0x8E);
    set_idt_entry(128, syscall_stub, 0x08, 0xEE);
}

void interrupt_handler(struct interrupt_frame *frame) {
    if(frame->int_no == 64) {
        isr_timer_modified(frame);
    }
    if(frame->int_no == 128) {
        syscall_handler(frame);
    }
}