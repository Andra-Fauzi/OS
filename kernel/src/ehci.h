#pragma once
#include "pci.h"
#include "paging.h"
#include "memory.h"
#include "terminal.h"
#include "main.h"

typedef volatile struct capability_regs {
    uint8_t cap_length;
    uint8_t reserved;
    uint16_t hci_version;
    uint32_t hcs_params;
    uint32_t hcc_params;
    uint32_t hcsp_portroute;
} __attribute__((packed)) ehci_capability_regs_t;

typedef volatile struct operation_regs {
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

typedef volatile struct queue_head {
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

typedef volatile struct usb_device_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t bcd_usb;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size0;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t bcd_device;
    uint8_t manufacturer;
    uint8_t product;
    uint8_t serial_number;
    uint8_t num_configurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef volatile struct usb_config_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t configuration_value;
    uint8_t configuration;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed)) usb_config_descriptor_t;

typedef volatile struct usb_interface_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef volatile struct usb_endpoint_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

typedef volatile struct queue_element_transfer_descriptor {
    uint32_t next_qtd;
    uint32_t alt_next_qtd;
    uint32_t token;
    uint32_t buffer[5];
} __attribute__((packed, aligned(32))) ehci_qtd_t;

typedef volatile struct EHCI {

} EHCI_t;

bool find_device_port();
void setup_ehci();
void setup_mouse_ehci();
void input_mouse_ehci();
void find_device_port_and_sign_address();