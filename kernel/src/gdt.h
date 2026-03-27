#pragma once
#include <stdint.h>

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
};

struct __attribute__((packed)) gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;

    uint32_t base_upper;
    uint32_t reserved;
};

struct __attribute__((packed)) tss64 {
    uint32_t reserved0;

    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;

    uint64_t reserved1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t reserved2;

    uint16_t reserved3;
    uint16_t iopb_offset;
};


struct __attribute__((packed)) gdtr {
    uint16_t limit;
    uint64_t base;
} ;

extern struct gdt_entry gdt[7];
extern struct gdt_tss_entry tss_desc;
extern struct tss64 tss;

void gdt_init();
void gdt_reload(struct gdtr* gdt_ptr);
void tss_load();
void update_tss_rsp0(uint64_t rsp0);
