#include "cpu.h"
#include <stdint.h>

void cpu_init(void) {
    uint64_t cr0, cr4;

    // 1. Enable FPU/SSE support in CR0
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Clear CR0.EM (Emulation bit) to allow x87 FPU
    cr0 |= (1ULL << 1);  // Set CR0.MP (Monitor co-processor)
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    // 2. Enable SSE support in CR4
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  // Set CR4.OSFXSR: bit 9 (FXSAVE and FXRSTOR)
    cr4 |= (1ULL << 10); // Set CR4.OSXMMEXCPT: bit 10 (SIMD Exception)
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}
