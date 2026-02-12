#include "gdt.h"
#include "util.h"

struct gdt_entry gdt[9];
struct gdt_tss_entry tss_desc;
struct tss64 tss;

void set_gdt_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low = (base & 0xFFFF);
    gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;

    gdt[i].limit_low = (limit & 0xFFFF);
    gdt[i].granularity = ((limit >> 16) & 0x0F);

    gdt[i].granularity |= (gran & 0xF0);
    gdt[i].access = access;
}

void set_gdt_tss_entry(int i, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // TSS is 16 bytes in 64-bit mode, taking 2 slots in GDT
    struct gdt_tss_entry *desc = (struct gdt_tss_entry *)&gdt[i];
    
    desc->limit_low = (limit & 0xFFFF);
    desc->base_low = (base & 0xFFFF);
    desc->base_mid = (base >> 16) & 0xFF;
    desc->access = access;
    desc->granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    desc->base_high = (base >> 24) & 0xFF;
    desc->base_upper = (base >> 32) & 0xFFFFFFFF;
    desc->reserved = 0;
}

void gdt_init() {
    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    // 0x00: Null
    set_gdt_entry(0, 0, 0, 0, 0);

    // 0x08: 16-bit Code
    set_gdt_entry(1, 0, 0xFFFF, 0x9A, 0x00);
    // 0x10: 16-bit Data
    set_gdt_entry(2, 0, 0xFFFF, 0x92, 0x00);
    // 0x18: 32-bit Code
    set_gdt_entry(3, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    // 0x20: 32-bit Data
    set_gdt_entry(4, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 0x28: 64-bit Code
    set_gdt_entry(5, 0, 0, 0x9A, 0x20);

    // 0x30: 64-bit Data
    set_gdt_entry(6, 0, 0, 0x92, 0x00);

    // 0x38: TSS Descriptor (Occupies indices 7 and 8)
    // Access: 0x89 (Present, Ring 0, Available 64-bit TSS)
    set_gdt_tss_entry(7, (uint64_t)&tss, sizeof(tss) - 1, 0x89, 0x00);
    
    struct gdtr gdt_ptr;
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t)gdt;

    gdt_reload(&gdt_ptr);
    tss_load();
}
