#include "thread.h"

#define LAPIC_EOI      0xB0

void isr_timer_modified(struct interrupt_frame *frame) {
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