#include "keyboard.h"

#define KEYBOARD_BUFFER_SIZE 256
#define LAPIC_EOI      0xB0


unsigned char keyboard_map[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

volatile uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
volatile uint32_t kb_tail = 0;
volatile uint32_t kb_head = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

__attribute__((interrupt))
void keyboard_callback(void *frame) {

	/*
	uint8_t status = inb(0x64);

	if(status & 0x01) {
		uint8_t scancode = inb(0x60);
		if(scancode & 0x80)
			return;
		uint32_t next = (kb_head + 1) % KEYBOARD_BUFFER_SIZE;
		if(next != kb_tail) {
			keyboard_buffer[kb_head] = scancode;
			kb_head = next;
		}
	}
	send_eoi(1);
	*/
	uint8_t sc = inb(0x60);
	if(!(sc & 0x80))
	{
		uint32_t next = (kb_head + 1) % KEYBOARD_BUFFER_SIZE;
		if(next != kb_tail) {
		keyboard_buffer[kb_head] = sc;
		kb_head = next;
		}
	}
    lapic_write(LAPIC_EOI, 0);
}

char keyboard_getchar() {
	if (kb_head == kb_tail) return -1;

	uint8_t scancode = keyboard_buffer[kb_tail];
	kb_tail = (kb_tail + 1) % KEYBOARD_BUFFER_SIZE;

	return keyboard_map[scancode];
}

void init_keyboard() {
	// Drain the PS/2 buffer
	while (inb(0x64) & 1) {
		inb(0x60);
	}

	// Enable the first PS/2 port (keyboard)
	outb(0x64, 0xAE);

	// Get the configuration byte
	outb(0x64, 0x20);
	while (!(inb(0x64) & 1));
	uint8_t config = inb(0x60);

	// Enable interrupts for the first port
	config |= (1 << 0);
	
	// Write back the configuration byte
	outb(0x64, 0x60);
	while (inb(0x64) & 2);
	outb(0x60, config);

	ioapic_enable_keyboard();
}
