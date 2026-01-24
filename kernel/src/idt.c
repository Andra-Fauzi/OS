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

void set_idt_entry(int n, void* handler) {
    uint64_t addr = (uint64_t)handler;
    idt[n].offset_low = addr & 0xFFFF;
    idt[n].selector = 0x28;
    idt[n].ist = 0;
    idt[n].type_attr = 0x8E;
    idt[n].offset_mid = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

void idt_init() {
    for (int i = 0; i < 256; i++) {
        set_idt_entry(i, isr_default);
    }

    /* exceptions WITH error code */
    set_idt_entry(8,  isr_default_err);
    set_idt_entry(10, isr_default_err);
    set_idt_entry(11, isr_default_err);
    set_idt_entry(12, isr_default_err);
    set_idt_entry(13, isr_default_err);
    set_idt_entry(14, isr_default_err);
    set_idt_entry(17, isr_default_err);

    /* APIC */
    set_idt_entry(0x40, isr_timer);
    set_idt_entry(0xFF, isr_spurious);
    set_idt_entry(33, keyboard_callback);

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;
    asm volatile ("lidt %0" : : "m"(idtp));
}

