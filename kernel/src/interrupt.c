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
            break;
        case 0:
            sys_read(frame);
            break;
        case 1:
            sys_write(frame);
            break;
        case 2:
            sys_open(frame);
            break;
        case 3:
            sys_close(frame);
            break;
        case 4:
            sys_stat(frame);
            break;
        case 5:
            sys_fstat(frame);
            break;
        case 8:
            sys_lseek(frame);
            break;
        case 7:
            sys_waitpid(frame);
        case 57:
            sys_fork(frame);
            break;
        case 59:
            sys_execve(frame);
            break;
        case 9:
            sys_mmap(frame);
            break;
        case 32:
            sys_dup(frame);
            break;
        case 33:
            sys_dup2(frame);
            break;
        case 22:
            sys_pipe(frame);
            break;
        case 80:
            sys_chdir(frame);
            break;
        case 183:
            sys_getcwd(frame);
            break;
        case 67:
            sys_sigaction(frame);
            break;
        case 62:
            sys_kill(frame);
            break;
        case 35:
            sys_nanosleep(frame);
            break;
        case 228:
            sys_clock_gettime(frame);
            break;
        case 16:
            sys_ioctl(frame);
            break;
        case 72:
            sys_fcntl(frame);
            break;
    }
}

void isr_timer_modified(struct interrupt_frame *frame) {
    tick++;
    lapic_write(LAPIC_EOI, 0);
    // printf("berubah\n");
    switch_thread(frame);
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