#include "idt.h"

struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct IDTEntry idt[256];
struct IDTPtr idtp;

void set_idt_entry(int n, void* handler, uint16_t selector, uint16_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = selector; 
    idt[n].ist = 0;
    idt[n].type_attr = type_attr;
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

void idt_init() {
    for (int i = 0; i < 256; i++) {
        set_idt_entry(i, isr_default, 0x28, 0x8E);
    }

    /* exceptions WITH error code */
    set_idt_entry(8,  isr_default_err, 0x28, 0x8E);
    set_idt_entry(10, isr_default_err, 0x28, 0x8E);
    set_idt_entry(11, isr_default_err, 0x28, 0x8E);
    set_idt_entry(12, isr_default_err, 0x28, 0x8E);
    set_idt_entry(13, isr_default_err, 0x28, 0x8E);
    set_idt_entry(14, isr_default_err, 0x28, 0x8E);
    set_idt_entry(17, isr_default_err, 0x28, 0x8E);

    /* APIC */
    set_idt_entry(0x40, isr_timer, 0x28, 0x8E);
    set_idt_entry(0xFF, isr_spurious, 0x28, 0x8E);
    set_idt_entry(33, keyboard_callback, 0x28, 0x8E);

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;
    asm volatile ("lidt %0" : : "m"(idtp));
}

