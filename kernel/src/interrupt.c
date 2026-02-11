#include "thread.h"

#define LAPIC_EOI      0xB0

static volatile uint64_t tick = 0;

void sleep(uint64_t ms) {
	uint64_t target = tick + ms;
	while (tick < target) {
		asm volatile("hlt");
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
    set_idt_entry(0x40, timer_stub, 0x28, 0x8E);
}

void interrupt_handler(struct interrupt_frame *frame) {
    if(frame->int_no == 64) {
        isr_timer_modified(frame);
    }
}