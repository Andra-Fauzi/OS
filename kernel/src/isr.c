#include "isr.h"

void isr_install() {
    set_idt_entry(0, (void *)isr0, 0x08, 0x8E);
    set_idt_entry(1, (void *)isr1, 0x08, 0x8E);
    set_idt_entry(2, (void *)isr2, 0x08, 0x8E);
    set_idt_entry(3, (void *)isr3, 0x08, 0x8E);
    set_idt_entry(4, (void *)isr4, 0x08, 0x8E);
    set_idt_entry(5, (void *)isr5, 0x08, 0x8E);
    set_idt_entry(6, (void *)isr6, 0x08, 0x8E);
    set_idt_entry(7, (void *)isr7, 0x08, 0x8E);
    set_idt_entry(8, (void *)isr8, 0x08, 0x8E);
    set_idt_entry(9, (void *)isr9, 0x08, 0x8E);
    set_idt_entry(10, (void *)isr10, 0x08, 0x8E);
    set_idt_entry(11, (void *)isr11, 0x08, 0x8E);
    set_idt_entry(12, (void *)isr12, 0x08, 0x8E);
    set_idt_entry(13, (void *)isr13, 0x08, 0x8E);
    set_idt_entry(14, (void *)isr14, 0x08, 0x8E);
    set_idt_entry(15, (void *)isr15, 0x08, 0x8E);
    set_idt_entry(16, (void *)isr16, 0x08, 0x8E);
    set_idt_entry(17, (void *)isr17, 0x08, 0x8E);
    set_idt_entry(18, (void *)isr18, 0x08, 0x8E);
    set_idt_entry(19, (void *)isr19, 0x08, 0x8E);
    set_idt_entry(20, (void *)isr20, 0x08, 0x8E);
    set_idt_entry(21, (void *)isr21, 0x08, 0x8E);
    set_idt_entry(22, (void *)isr22, 0x08, 0x8E);
    set_idt_entry(23, (void *)isr23, 0x08, 0x8E);
    set_idt_entry(24, (void *)isr24, 0x08, 0x8E);
    set_idt_entry(25, (void *)isr25, 0x08, 0x8E);
    set_idt_entry(26, (void *)isr26, 0x08, 0x8E);
    set_idt_entry(27, (void *)isr27, 0x08, 0x8E);
    set_idt_entry(28, (void *)isr28, 0x08, 0x8E);
    set_idt_entry(29, (void *)isr29, 0x08, 0x8E);
    set_idt_entry(30, (void *)isr30, 0x08, 0x8E);
    set_idt_entry(31, (void *)isr31, 0x08, 0x8E);
}

void isr_handler(struct interrupt_frame* frame) {
    printf("Interrupt %d\n", frame->int_no);
    printf("RIP : %x\n", frame->rip);
    while(1) asm volatile("hlt");
}
