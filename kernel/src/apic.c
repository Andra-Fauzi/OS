#include "apic.h"
#include <stdint.h>
#include <limine.h>

/* =======================
   I/O PORT (PIC)
   ======================= */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* =======================
   MSR
   ======================= */
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void write_msr(uint32_t msr, uint64_t val) {
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr),
          "a"((uint32_t)val),
          "d"((uint32_t)(val >> 32))
    );
}

/* =======================
   LIMINE HHDM
   ======================= */
extern volatile struct limine_hhdm_request hhdm_request;

/* =======================
   LAPIC DEFINITIONS
   ======================= */
#define IA32_APIC_BASE 0x1B

#define LAPIC_TPR      0x80
#define LAPIC_EOI      0xB0
#define LAPIC_SIVR     0xF0
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_TIMER_INIT 0x380
#define LAPIC_TIMER_DIV  0x3E0

#define LAPIC_TIMER_DIV  0x3E0
#define IOAPIC_BASE 0xFEC00000

// the importants APIC registers
volatile uint32_t *lapic = 0;
volatile uint32_t *ioapic = 0;

void lapic_write(uint32_t reg, uint32_t val) {
    lapic[reg / 4] = val;
}

uint32_t lapic_read(uint32_t reg) {
    return lapic[reg / 4];
}

/* =======================
   DISABLE PIC
   ======================= */
void pic_disable(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

extern uint64_t *get_pml4(void);
extern void map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);

#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_NOCACHE  (1ULL << 4)

/* =======================
   ENABLE xAPIC (MMIO)
   ======================= */
void lapic_enable(void) {
    uint64_t apic_base = read_msr(IA32_APIC_BASE);

    /* makes sure x2APIC OFF */
    apic_base &= ~(1ULL << 10);

    /* enable xAPIC */
    apic_base |= (1ULL << 11);
    write_msr(IA32_APIC_BASE, apic_base);

    uint64_t phys = apic_base & 0xFFFFF000ULL;
    uint64_t virt = phys + hhdm_request.response->offset;

    /* MAP APIC MMIO */
    uint64_t *pml4 = get_pml4();
    map_page(
        pml4,
        virt,
        phys,
        PTE_PRESENT | PTE_WRITABLE | PTE_NOCACHE
    );

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");

    lapic = (volatile uint32_t *)virt;

    /* MAP IOAPIC MMIO (Fixed 0xFEC00000) */
    uint64_t ioapic_phys = IOAPIC_BASE;
    uint64_t ioapic_virt = ioapic_phys + hhdm_request.response->offset;
    map_page(
        pml4,
        ioapic_virt,
        ioapic_phys,
        PTE_PRESENT | PTE_WRITABLE | PTE_NOCACHE
    );
    asm volatile("invlpg (%0)" :: "r"(ioapic_virt) : "memory");
    ioapic = (volatile uint32_t *)ioapic_virt;

    /* NOW SAFE */
    lapic_write(LAPIC_TPR, 0);
    lapic_write(LAPIC_SIVR, 0x100 | 0xFF);
}


/* =======================
   APIC TIMER INIT
   ======================= */
void lapic_timer_init(void) {
    /* divide by 16 */
    lapic_write(LAPIC_TIMER_DIV, 0x3);

    /* periodic, vector 0x40 */
    lapic_write(LAPIC_LVT_TIMER, 0x20000 | 0x40);

    /* initial count (1 ms) */
    lapic_write(LAPIC_TIMER_INIT, 0x1000000);
}


/* =======================
   ISR
   ======================= */
// not use this anymore
   __attribute__((interrupt))
void isr_timer(void *frame) {
    (void)frame;

    /* DO NOT PRINT TOO OFTEN */
    if (1) {
        print_str("timer jalan\n");
    }

    lapic_write(LAPIC_EOI, 0);
}

__attribute__((interrupt))
void isr_spurious(void *frame) {
    (void)frame;
    lapic_write(LAPIC_EOI, 0);
}

__attribute__((interrupt))
void isr_default(void *frame) {
    (void)frame;
    print_str("\nUNHANDLED INTERRUPT\n");
    for (;;) asm volatile("hlt");
}

__attribute__((interrupt))
void isr_default_err(void *frame, uint64_t error) {
    (void)frame;
    print_str("ISR (error code): ");
    print_hex(error);
    print_str("\n");
    for (;;) asm volatile("hlt");
}

// SPECIFIC 

void ioapic_write(uint32_t reg, uint32_t val) {
    // IOREGSEL is at offset 0x00
    // IOWIN    is at offset 0x10
    ioapic[0] = reg;
    ioapic[4] = val; // 0x10 / 4 = 4
}

void ioapic_enable_keyboard() {
	uint8_t irq = 1;
	uint8_t vector = 0x21;

	uint32_t low = 
		vector |
		(0 << 8) | // delivery = fixed
		(0 << 11) | // physical mode
		(0 << 13) | // active_high
		(0 << 15); // unmasked
			   //
			   //
	uint32_t high = 0 << 24; // target cpu 0
	
	// IOREDTBL starts at index 0x10
    // Redirection entry is 64-bits (2x 32-bit registers)
    // Index = 0x10 + irq * 2
    
    ioapic_write(0x10 + irq * 2, low);
    ioapic_write(0x10 + irq * 2 + 1, high);
}
