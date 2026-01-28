#pragma once
#include "pci.h"
#include "paging.h"
#include "memory.h"
#include "terminal.h"
#include "main.h"

typedef struct capability_regs {
    uint8_t cap_length;
    uint8_t reserved;
    uint16_t hci_version;
    uint32_t hcs_params;
    uint32_t hcc_params;
    uint32_t hcsp_portroute;
} __attribute__((packed)) ehci_capability_regs_t;

typedef struct operation_regs {
    uint32_t usb_command;
    uint32_t usb_status;
    uint32_t usb_interrupt;
    uint32_t frame_index;
    uint32_t ctrl_ds_segment;
    uint32_t periodic_list_base;
    uint32_t async_list_addr;
    uint32_t reserved[9];
    uint32_t config_flag;
    uint32_t port_status_or_control[];
} __attribute__((packed)) ehci_operation_regs_t;

typedef struct queue_head {
    uint32_t horizontal_link_pointer; // for circular queue_heads
    uint32_t endpoint_characteristics;
    uint32_t endpoint_capabilities;
    uint32_t current_qtd_address;
    // this below is overlay that copy active qTD to here
    uint32_t next_qtd;
    uint32_t alt_next_qtd;
    uint32_t token;
    uint32_t buffer[5];
} __attribute__((packed, aligned(32))) ehci_queue_head_t;

typedef struct queue_element_transfer_descriptor {
    uint32_t next_qtd;
    uint32_t alt_next_qtd;
    uint32_t token;
    uint32_t buffer[5];
} __attribute__((packed, aligned(32))) ehci_qtd_t;

bool find_device_port();
void setup_ehci();
void setup_mouse_ehci();