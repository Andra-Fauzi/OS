#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "paging.h"
#include "pci.h"
#include "memory.h"

typedef volatile struct ohci_hhca {
	uint32_t interrupt_table[32];
	uint16_t frame_number;
	uint16_t pad1;
	uint32_t done_head;
	uint8_t reserved[116];
} __attribute__((packed)) ohci_hhca_t;

typedef volatile struct ohci_ed {
	uint32_t flags;
	uint32_t tail_pointer;
	uint32_t head_pointer;
	uint32_t next_ed;
} __attribute__((packed)) ohci_ed_t;

typedef volatile struct ochi_td {
	uint32_t flags;
	uint32_t current_buffer_ptr;
	uint32_t next_td;
	uint32_t buffer_end_ptr;
} __attribute__((packed)) ohci_td_t;

typedef volatile struct ohci_regs {
	uint32_t revision;
	uint32_t control;
	uint32_t command_status;
	uint32_t interrupt_status;
	uint32_t interrupt_enable;
	uint32_t interrupt_disable;
	uint32_t hhca;
	uint32_t period_current_ed;
	uint32_t control_head_ed;
	uint32_t control_current_ed;
	uint32_t bulk_head_ed;
	uint32_t bulk_current_ed;
	uint32_t done_head;
	uint32_t frame_interval;
	uint32_t frame_remaining;
	uint32_t frame_number;
	uint32_t periodic_start;
	uint32_t low_speed_threshold;
	uint32_t root_hub_descriptor_A;
	uint32_t root_hub_descriptor_B;
	uint32_t root_hub_status;
	uint32_t root_hub_port_status[15];
} __attribute__((packed)) ohci_regs_t;

extern volatile ohci_regs_t *ohci_regs;

bool find_ohci();
void init_OHCI();
void check_device_status();
void setup_mouse();
void input_mouse();
